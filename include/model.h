#ifndef __MODEL_H__
#define __MODEL_H__

#include "simple_json.h"
#include "gf3d_obj_load.h"
#include "gf3d_mesh.h"



typedef struct 
{
    GFC_Matrix4 model;
    GFC_Matrix4 view;
    GFC_Matrix4 proj;
    GFC_Vector4D color;
}ModelUBO;

typedef struct 
{
    int                         _refCount;
    Texture                    *texture;
    Mesh                       *mesh;
}Model;

void model_init_system(Uint32 modelCount);

/**
 * @brief get the pipeline that is used to render basic 3d meshes
 * @return NULL on error or the pipeline in question
 */
Pipeline *model_get_pipeline();



/**
 * @brief load a model from a file
 * @param filename the name of the model file to load
 */  
Model *model_load(const char *filename);

/**
 * @brief get a model by filename
 * @param filename the name of the model to get
 * @return NULL on error or the model otherwise
 */
void model_free(Model *model);

Model model_queue_render_sky(Model *model, GFC_Matrix4 modelMat, GFC_Color colorMod);

Model model_queue_render(Model *model, GFC_Matrix4 modelMat, GFC_Color colorMod);



#endif
