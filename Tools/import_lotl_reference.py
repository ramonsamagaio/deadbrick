import os
import re
import unreal

PROJECT_ROOT = os.path.abspath(unreal.Paths.project_dir())
SOURCE_ROOT = os.path.join(PROJECT_ROOT, "ReferenceExported")
DEST_ROOT = "/Game/ReferenceImported"
MARKER = os.path.join(PROJECT_ROOT, "Saved", "LOTL_EDITOR_IMPORT.txt")
SUPPORTED = {".glb", ".gltf", ".png", ".tga", ".jpg", ".jpeg"}


def sanitize_segment(value: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", value)
    value = re.sub(r"_+", "_", value).strip("_")
    return value or "Reference"


def destination_for(filename: str) -> str:
    parent = os.path.relpath(os.path.dirname(filename), SOURCE_ROOT)
    if parent in (".", ""):
        return DEST_ROOT
    parts = [sanitize_segment(part) for part in parent.replace("\\", "/").split("/") if part not in ("", ".")]
    return DEST_ROOT + "/" + "/".join(parts)


def collect_source_files():
    result = []
    if not os.path.isdir(SOURCE_ROOT):
        return result
    for root, _, files in os.walk(SOURCE_ROOT):
        for name in files:
            ext = os.path.splitext(name)[1].lower()
            if ext in SUPPORTED:
                result.append(os.path.join(root, name))
    # Import mesh containers first, then loose textures. This lets Interchange establish mesh/material
    # relationships before any remaining texture-only packages are added.
    result.sort(key=lambda path: (0 if os.path.splitext(path)[1].lower() in (".glb", ".gltf") else 1, path.lower()))
    return result


def run():
    files = collect_source_files()
    os.makedirs(os.path.dirname(MARKER), exist_ok=True)

    if not files:
        message = "DEADBRICK LOTL import skipped: ReferenceExported contains no GLB/GLTF/texture files."
        unreal.log_warning(message)
        with open(MARKER, "w", encoding="utf-8") as handle:
            handle.write(message + "\n")
        return

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for filename in files:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", filename)
        task.set_editor_property("destination_path", destination_for(filename))
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("replace_existing_settings", False)
        task.set_editor_property("save", True)
        try:
            task.set_editor_property("async_", False)
        except Exception:
            pass
        tasks.append(task)

    unreal.log("DEADBRICK LOTL: importing {} editor-safe reference files...".format(len(tasks)))
    asset_tools.import_asset_tasks(tasks)

    imported_paths = []
    failed_files = []
    for task in tasks:
        paths = list(task.get_editor_property("imported_object_paths"))
        if not paths:
            try:
                paths = [obj.get_path_name() for obj in task.get_objects() if obj]
            except Exception:
                paths = []
        if paths:
            imported_paths.extend(paths)
        else:
            failed_files.append(task.get_editor_property("filename"))

    imported_paths = sorted(set(imported_paths))
    try:
        unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=True, recursive=True)
    except Exception as exc:
        unreal.log_warning("DEADBRICK LOTL: save_directory warning: {}".format(exc))

    lines = [
        "DEADBRICK - LOTL editor-safe import",
        "SourceRoot: {}".format(SOURCE_ROOT),
        "DestinationRoot: {}".format(DEST_ROOT),
        "SourceFiles: {}".format(len(files)),
        "ImportedObjectPaths: {}".format(len(imported_paths)),
        "FailedSourceFiles: {}".format(len(failed_files)),
        "",
        "=== IMPORTED OBJECTS ===",
    ]
    lines.extend(imported_paths)
    if failed_files:
        lines.append("")
        lines.append("=== FAILED SOURCE FILES ===")
        lines.extend(failed_files)

    with open(MARKER, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")

    unreal.log("DEADBRICK LOTL editor import complete: {} objects from {} files ({} failed files).".format(
        len(imported_paths), len(files), len(failed_files)))


run()
