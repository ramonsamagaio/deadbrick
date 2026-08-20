import os
import re
import unreal

PROJECT_ROOT = os.path.abspath(unreal.Paths.project_dir())
SOURCE_ROOT = os.path.join(PROJECT_ROOT, "ReferenceExported")
DEST_ROOT = "/Game/ReferenceImported"
MARKER = os.path.join(PROJECT_ROOT, "Saved", "LOTL_EDITOR_IMPORT.txt")
SUPPORTED = {".glb", ".gltf", ".psk", ".pskx", ".psa", ".png", ".tga", ".jpg", ".jpeg"}


def sanitize_segment(value: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", value)
    value = re.sub(r"_+", "_", value).strip("_")
    return value or "Reference"


def relative_source_parent(filename: str) -> str:
    parent = os.path.relpath(os.path.dirname(filename), SOURCE_ROOT)
    return "" if parent in (".", "") else parent.replace("\\", "/")


def destination_for(filename: str) -> str:
    parent = relative_source_parent(filename)
    if not parent:
        return DEST_ROOT
    parts = [sanitize_segment(part) for part in parent.split("/") if part not in ("", ".")]
    return DEST_ROOT + "/" + "/".join(parts)


def collect_source_files():
    result = []
    seen = set()
    if not os.path.isdir(SOURCE_ROOT):
        return result
    for root, _, files in os.walk(SOURCE_ROOT):
        for name in files:
            filename = os.path.abspath(os.path.join(root, name))
            ext = os.path.splitext(name)[1].lower()
            key = filename.lower()
            if ext in SUPPORTED and key not in seen:
                seen.add(key)
                result.append(filename)
    return sorted(result, key=str.lower)


def make_task(filename, factory=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", destination_for(filename))
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", True)
    if factory is not None:
        task.set_editor_property("factory", factory)
    try:
        task.set_editor_property("async_", False)
    except Exception:
        pass
    return task


def task_paths(task):
    paths = list(task.get_editor_property("imported_object_paths"))
    if paths:
        return paths
    try:
        return [obj.get_path_name() for obj in task.get_objects() if obj]
    except Exception:
        return []


def import_tasks(asset_tools, filenames):
    if not filenames:
        return [], []
    tasks = [make_task(filename) for filename in filenames]
    asset_tools.import_asset_tasks(tasks)
    imported = []
    failed = []
    for task in tasks:
        paths = task_paths(task)
        if paths:
            imported.extend(paths)
        else:
            failed.append(task.get_editor_property("filename"))
    return imported, failed


def common_prefix_segments(a: str, b: str) -> int:
    aa = [x.lower() for x in a.replace("\\", "/").split("/") if x]
    bb = [x.lower() for x in b.replace("\\", "/").split("/") if x]
    count = 0
    for left, right in zip(aa, bb):
        if left != right:
            break
        count += 1
    return count


def name_tokens(value: str):
    return set(token for token in re.split(r"[^a-z0-9]+", value.lower()) if len(token) >= 3)


def collect_skeleton_candidates(psk_files):
    candidates = []

    # Prefer meshes produced during this exact import because their source directory is known.
    for filename in psk_files:
        dest = destination_for(filename)
        asset_stem = sanitize_segment(os.path.splitext(os.path.basename(filename))[0].replace("_LOD0", ""))
        possible_path = dest + "/" + asset_stem
        obj = unreal.EditorAssetLibrary.load_asset(possible_path)
        if obj and isinstance(obj, unreal.SkeletalMesh):
            skeleton = obj.get_editor_property("skeleton")
            if skeleton:
                candidates.append({
                    "skeleton": skeleton,
                    "source_parent": relative_source_parent(filename),
                    "name": asset_stem,
                    "asset_path": obj.get_path_name(),
                })

    # Also include any already imported skeletal meshes, covering naming differences made by the factory.
    try:
        for asset_path in unreal.EditorAssetLibrary.list_assets(DEST_ROOT, recursive=True, include_folder=False):
            obj = unreal.EditorAssetLibrary.load_asset(asset_path)
            if not obj or not isinstance(obj, unreal.SkeletalMesh):
                continue
            skeleton = obj.get_editor_property("skeleton")
            if not skeleton:
                continue
            key = skeleton.get_path_name()
            if any(item["skeleton"].get_path_name() == key for item in candidates):
                continue
            package_path = asset_path.rsplit("/", 1)[0].replace(DEST_ROOT, "", 1).strip("/")
            candidates.append({
                "skeleton": skeleton,
                "source_parent": package_path,
                "name": obj.get_name(),
                "asset_path": obj.get_path_name(),
            })
    except Exception as exc:
        unreal.log_warning("DEADBRICK LOTL: could not enumerate prior skeletons: {}".format(exc))

    return candidates


def choose_skeleton(psa_filename, candidates):
    if not candidates:
        return None

    parent = relative_source_parent(psa_filename)
    stem = os.path.splitext(os.path.basename(psa_filename))[0]
    tokens = name_tokens(stem)
    best = None
    best_score = -1

    for item in candidates:
        prefix = common_prefix_segments(parent, item["source_parent"])
        overlap = len(tokens.intersection(name_tokens(item["name"])))
        same_leaf = 1 if parent and item["source_parent"] and parent.split("/")[-1].lower() == item["source_parent"].split("/")[-1].lower() else 0
        score = prefix * 100 + same_leaf * 40 + overlap * 10
        if score > best_score:
            best_score = score
            best = item

    return best


def create_psa_factory(skeleton):
    factory_class = unreal.load_class(None, "/Script/UnrealPSKPSA.PSAFactory")
    if not factory_class:
        raise RuntimeError("UnrealPSKPSA.PSAFactory is not loaded. Run SETUP_LOTL_ASSET_PIPELINE.bat and rebuild.")

    factory = unreal.new_object(factory_class)
    settings = factory.get_editor_property("settings_importer")
    if not settings:
        raise RuntimeError("PSAFactory did not expose SettingsImporter.")
    settings.set_editor_property("skeleton", skeleton)
    return factory


def import_psa_files(asset_tools, psa_files, skeleton_candidates):
    imported = []
    failed = []
    assignments = []

    for filename in psa_files:
        candidate = choose_skeleton(filename, skeleton_candidates)
        if not candidate:
            unreal.log_warning("DEADBRICK LOTL: no skeleton candidate for PSA {}".format(filename))
            failed.append(filename)
            continue

        try:
            factory = create_psa_factory(candidate["skeleton"])
            task = make_task(filename, factory)
            asset_tools.import_asset_tasks([task])
            paths = task_paths(task)
            if paths:
                imported.extend(paths)
                assignments.append("{} -> {}".format(os.path.relpath(filename, SOURCE_ROOT), candidate["asset_path"]))
            else:
                failed.append(filename)
        except Exception as exc:
            unreal.log_error("DEADBRICK LOTL: PSA import failed {}: {}".format(filename, exc))
            failed.append(filename)

    return imported, failed, assignments


def run():
    files = collect_source_files()
    os.makedirs(os.path.dirname(MARKER), exist_ok=True)

    if not files:
        message = "DEADBRICK LOTL import skipped: ReferenceExported contains no supported reference files."
        unreal.log_warning(message)
        with open(MARKER, "w", encoding="utf-8") as handle:
            handle.write(message + "\n")
        return

    by_ext = {}
    for filename in files:
        by_ext.setdefault(os.path.splitext(filename)[1].lower(), []).append(filename)

    # Phase order matters. PSK first creates Skeleton assets; PSA is imported last and receives the
    # nearest recovered LOTL skeleton explicitly, so the ActorX importer never opens a modal picker.
    psk_files = by_ext.get(".psk", [])
    pskx_files = by_ext.get(".pskx", [])
    gltf_files = by_ext.get(".glb", []) + by_ext.get(".gltf", [])
    texture_files = []
    for ext in (".png", ".tga", ".jpg", ".jpeg"):
        texture_files.extend(by_ext.get(ext, []))
    psa_files = by_ext.get(".psa", [])

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    imported_paths = []
    failed_files = []

    unreal.log("DEADBRICK LOTL: phase 1 importing {} skeletal PSK files...".format(len(psk_files)))
    imported, failed = import_tasks(asset_tools, psk_files)
    imported_paths.extend(imported)
    failed_files.extend(failed)

    unreal.log("DEADBRICK LOTL: phase 2 importing {} static PSKX/GLTF files...".format(len(pskx_files) + len(gltf_files)))
    imported, failed = import_tasks(asset_tools, pskx_files + gltf_files)
    imported_paths.extend(imported)
    failed_files.extend(failed)

    unreal.log("DEADBRICK LOTL: phase 3 importing {} loose textures...".format(len(texture_files)))
    imported, failed = import_tasks(asset_tools, texture_files)
    imported_paths.extend(imported)
    failed_files.extend(failed)

    skeleton_candidates = collect_skeleton_candidates(psk_files)
    unreal.log("DEADBRICK LOTL: {} skeleton candidates available for PSA matching.".format(len(skeleton_candidates)))

    unreal.log("DEADBRICK LOTL: phase 4 importing {} PSA animations...".format(len(psa_files)))
    imported, failed, assignments = import_psa_files(asset_tools, psa_files, skeleton_candidates)
    imported_paths.extend(imported)
    failed_files.extend(failed)

    imported_paths = sorted(set(imported_paths))
    failed_files = sorted(set(failed_files))

    try:
        unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=True, recursive=True)
    except Exception as exc:
        unreal.log_warning("DEADBRICK LOTL: save_directory warning: {}".format(exc))

    lines = [
        "DEADBRICK - LOTL editor-safe import",
        "SourceRoot: {}".format(SOURCE_ROOT),
        "DestinationRoot: {}".format(DEST_ROOT),
        "SourceFiles: {}".format(len(files)),
        "SkeletalPSK: {}".format(len(psk_files)),
        "StaticPSKX: {}".format(len(pskx_files)),
        "StaticGLTF: {}".format(len(gltf_files)),
        "PSAAnimations: {}".format(len(psa_files)),
        "SkeletonCandidates: {}".format(len(skeleton_candidates)),
        "ImportedObjectPaths: {}".format(len(imported_paths)),
        "FailedSourceFiles: {}".format(len(failed_files)),
        "",
        "=== PSA SKELETON ASSIGNMENTS ===",
    ]
    lines.extend(assignments)
    lines.append("")
    lines.append("=== IMPORTED OBJECTS ===")
    lines.extend(imported_paths)
    if failed_files:
        lines.append("")
        lines.append("=== FAILED SOURCE FILES ===")
        lines.extend(failed_files)

    with open(MARKER, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")

    unreal.log("DEADBRICK LOTL editor import complete: {} objects from {} files ({} failed files, {} PSA assignments).".format(
        len(imported_paths), len(files), len(failed_files), len(assignments)))


run()
