
#include "model.h"

typedef struct
{
    GFC_Matrix4 model;
    GFC_Matrix4 view;
    GFC_Matrix4 proj;
    GFC_Color colorMod;
} ModelUBO;



typedef struct
{
    Model           *modelList;
    Uint32           modelCount;
    Pipeline        *pipe;
    Pipeline        *skyPipe;
    Texture         *texture;
    VkDevice         device;
}ModelManager;

static ModelManager model_manager = {0};

void model_init_system(Uint32 modelCount)
{
    if (model_manager.modelCount != 0)
    {
        slog("cannot initialize model system, already initialized");
        return;
    }
    if (modelCount == 0)
    {
        slog("cannot initialize model system with 0 models");
        return;
    }
    model_manager.modelList = gfc_allocate_array(sizeof(Model),modelCount);
    model_manager.modelCount = modelCount;
    model_manager.pipe = gf3d_pipeline_create_from_config(
        model_manager.device,
        "config/model_pipeline.cfg",
        gf3d_vgraphics_get_view_extent(),
        modelCount,
        gf3d_mesh_get_bind_description(),
        gf3d_mesh_get_attribute_descriptions(NULL),
        3,
        sizeof(ModelUBO),
        VK_INDEX_TYPE_UINT32);
    model_manager.skyPipe = gf3d_pipeline_load("config/sky_pipeline.cfg");
    atexit(model_close);
}

Model *model_load(const char *filename)
{

    const char *str = NULL;
    const char *str2 = NULL;
    SJson *json;
    SJson *data;
    Mesh *mesh;
    Texture *texture;


    Model *model;
    if (!filename)return NULL;
    model = model_get_by_filename(filename);
    if(model)
    {
        model->_refCount++;
        return model;
    }
    json = sjson_load(filename);
    if(!json)
    {
        slog("failed to load model");
        return NULL;
    }
    data = sj_object_get_value(json,"model");
    if(!data)
    {
        slog("model file missing model data");
        sj_free(json);
        return NULL;
    }
    str = sj_object_get_string(data,"obj");
    if(!str)
    {
        slog("model file missing obj data");
        sj_free(json);
        return NULL;
    }
    mesh = gf3d_mesh_load_obj(str);
    if(!mesh)
    {
        slog("failed to load mesh for model");
        sj_free(json);
        return NULL;
    }
    str2 = sj_object_get_string(data,"texture");
    if(!str2)
    {
        slog("model file missing texture data");
        sj_free(json);
        return NULL;
    }
    texture = gf3d_texture_load(str2);
    if(!texture)
    {
        slog("failed to load texture for model");
        sj_free(json);
        return NULL;
    } 
    

}


Model *model_get_by_filename(const char *filename)
{
    int i;
    Model *model;
    if (!filename)return NULL;
    for (i = 0; i < model_manager.modelCount;i++)
    {
        model = &model_manager.modelList[i];
        if (model->_refCount == 0)continue;
        if (strcmp(model->mesh->filename,filename) == 0)
        {
            return model;
        }
    }
    return NULL;
}

void model_close()
{
    int i,c;
    for(i = 0; i < model_manager.modelCount; i++)
    {
        model_free(&model_manager.modelList[i]);
    }


}
Pipeline *model_get_pipeline()
{
    return model_manager.pipe;
}

ModelUBO model_get_ubo(
    GFC_Matrix4 modelMat,
    GFC_Color colorMod
)
{
    ModelUBO ubo = {0};
    GFC_Matrix4 *view;
    gfc_matrix4_copy(ubo.model, modelMat);
    view = gf3d_vgraphics_get_view_matrix();
    if (view)gfc_matrix4_copy(ubo.view, view);

    gf3d_vgraphics_get_projection_matrix(&ubo.proj);
    return ubo;
}
Model model_free(Model *model)
{
    if(!model)return;
    model->_refCount--;

}
Model *gf3d_model_new()
{
    int i;
    for (i = 0; i < model_manager.modelCount;i++)
    {
        if (model_manager.modelList[i]._refCount == 0)
        {
            model_manager.modelList[i]._refCount = 1;
            model_manager.device = VKDevice();
            return &model_manager.modelList[i];
        }
    }
    return NULL;
}
void model_delete(Model *model)
{
    if (!model)return;

    gf3d_texture_free(model->texture);
    gf3d_mesh_free(model->mesh);
}
void model_queue_render_sky(Model *model, GFC_Matrix4 modelMat, GFC_Color colorMod)
{
    ModelUBO ubo;
    if (!model)return;
    ubo = model_get_ubo(modelMat, colorMod);
    gf3d_mesh_queue_render(model->mesh, model_manager.skyPipe, &ubo, model->texture);
}
void model_queue_render(Model *model, GFC_Matrix4 modelMat, GFC_Color colorMod)
{
    ModelUBO ubo;
    if (!model)return;
    ubo = model_get_ubo(modelMat, colorMod);
    gf3d_mesh_queue_render(model->mesh, model_manager.pipe, &ubo, model->texture);
}