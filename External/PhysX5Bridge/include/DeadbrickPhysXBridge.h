#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DBPXScene DBPXScene;

typedef struct DBPXTransform
{
    float X;
    float Y;
    float Z;
    float QX;
    float QY;
    float QZ;
    float QW;
} DBPXTransform;

DBPXScene* DBPX_CreateScene(float GroundHeightCm);
void DBPX_DestroyScene(DBPXScene* Scene);
int32_t DBPX_IsReady(const DBPXScene* Scene);
void DBPX_Simulate(DBPXScene* Scene, float DeltaSeconds);
void DBPX_SetGroundHeight(DBPXScene* Scene, float GroundHeightCm);

int64_t DBPX_CreateDynamicBox(
    DBPXScene* Scene,
    const DBPXTransform* InitialTransform,
    float HalfExtentX,
    float HalfExtentY,
    float HalfExtentZ,
    float MassKg,
    float LinearDamping,
    float AngularDamping);

void DBPX_DestroyBody(DBPXScene* Scene, int64_t Handle);
int32_t DBPX_GetBodyTransform(DBPXScene* Scene, int64_t Handle, DBPXTransform* OutTransform);
int32_t DBPX_AddImpulse(DBPXScene* Scene, int64_t Handle, float X, float Y, float Z);

#ifdef __cplusplus
}
#endif
