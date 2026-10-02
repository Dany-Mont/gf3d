#include "simple_json.h"
#include "simple_logger.h"

#include "gfc_types.h"

#include "gf3d_buffers.h"
#include "gf3d_swapchain.h"
#include "gf3d_vgraphics.h"
#include "gf3d_pipeline.h"
#include "gf3d_commands.h"
#include "gf3d_obj_load.h"

#include "model.h"

extern int __DEBUG;

typedef struct
{
    Model           *modelList;
    Uint32           modelCount;
    Uint32           chainLength;
    Pipeline        *pipe;
    Pipeline        *skyPipe;
    Texture         *defaultTexture;
    VkDevice         device;
}ModelManager;

static ModelManager model_manager = {0};

//forward declarations start
void model_delete(Model *model);
void model_system_close();
Model *model_get_by_filename(const char *filename);
//forward declarations end

void model_manager_init(Uint32 modelCount)
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
    model_manager.chainLength = gf3d_swapchain_get_chain_length();
    model_manager.device = gf3d_vgraphics_get_default_logical_device();
    model_manager.modelList = gfc_allocate_array(sizeof(Model),modelCount);
    if (!model_manager.modelList)
    {
        //gfc_allocate_array already logged the error
        return;
    }
    model_manager.modelCount = modelCount;

    //the model system owns the mesh system, it has to exist before any model loads
    gf3d_mesh_system_init(1024);

    //sky pipeline, not set up yet. model_queue_render_sky does nothing while this is NULL
    /*
    model_manager.skyPipe = gf3d_pipeline_create_from_config(
        model_manager.device,
        "config/sky_pipeline.cfg",
        gf3d_vgraphics_get_view_extent(),
        10,
        gf3d_mesh_get_bind_description(),
        gf3d_mesh_get_attribute_descriptions(NULL),
        MESH_ATTRIBUTE_COUNT,
        sizeof(ModelUBO),
        VK_INDEX_TYPE_UINT16);
    */

    model_manager.pipe = gf3d_pipeline_create_from_config(
        model_manager.device,
        "config/model_pipeline.cfg",
        gf3d_vgraphics_get_view_extent(),
        modelCount,
        gf3d_mesh_get_bind_description(),
        gf3d_mesh_get_attribute_descriptions(NULL),
        MESH_ATTRIBUTE_COUNT,
        sizeof(ModelUBO),
        VK_INDEX_TYPE_UINT16);//must match the Face index type in gf3d_mesh.h
    if (!model_manager.pipe)
    {
        slog("failed to make pipeline for models");
        slog_sync();
        model_system_close();
        exit(-1);
        return;
    }

    model_manager.defaultTexture = gf3d_texture_load("images/default.png");
    if (!model_manager.defaultTexture)
    {
        slog("default texture images/default.png failed to load, models without a texture will not render");
        slog_sync();
    }
    if (__DEBUG)slog("model manager initialized");
    atexit(model_system_close);
}

void model_system_close()
{
    int i;
    if (model_manager.modelList)
    {
        for (i = 0; i < model_manager.modelCount; i++)
        {
            model_delete(&model_manager.modelList[i]);
        }
        free(model_manager.modelList);
    }
    if (model_manager.defaultTexture)
    {
        gf3d_texture_free(model_manager.defaultTexture);
    }
    memset(&model_manager,0,sizeof(ModelManager));
    if (__DEBUG)slog("model manager closed");
}

Model *model_new()
{
    int i;
    int emptyIndex = -1;
    for (i = 0; i < model_manager.modelCount; i++)
    {
        if (model_manager.modelList[i]._refCount != 0)continue;
        if (emptyIndex < 0)emptyIndex = i;
        if (strlen(model_manager.modelList[i].filename) == 0)
        {
            //never used slot, take it
            model_manager.modelList[i]._refCount = 1;
            return &model_manager.modelList[i];
        }
    }
    if (emptyIndex < 0)
    {
        slog("model_new: no free slots for new models");
        return NULL;
    }
    //every slot has been used once, recycle the first one nobody is using
    model_delete(&model_manager.modelList[emptyIndex]);
    model_manager.modelList[emptyIndex]._refCount = 1;
    return &model_manager.modelList[emptyIndex];
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
    json = sj_load(filename);
    if(!json)
    {
        slog("failed to load model file %s",filename);
        return NULL;
    }
    data = sj_object_get_value(json,"model");
    if(!data)
    {
        slog("model file %s missing model data",filename);
        sj_free(json);
        return NULL;
    }
    str = sj_object_get_string(data,"obj");
    if(!str)
    {
        slog("model file %s missing obj data",filename);
        sj_free(json);
        return NULL;
    }
    mesh = gf3d_mesh_load_obj(str);
    if(!mesh)
    {
        slog("failed to load mesh for model file %s",filename);
        sj_free(json);
        return NULL;
    }
    //a missing or broken texture falls back to the default one
    texture = NULL;
    str2 = sj_object_get_string(data,"texture");
    if(str2)
    {
        texture = gf3d_texture_load(str2);
        if(!texture)
        {
            slog("failed to load texture %s for model file %s, using default",str2,filename);
        }
    }
    if(!texture)
    {
        texture = gf3d_texture_load("images/default.png");//reloading bumps the ref count so the free in model_delete balances
    }
    sj_free(json);//everything we needed has been copied out of it

    model = model_new();
    if(!model)
    {
        slog("failed to get an empty space in memory for model %s",filename);
        gf3d_mesh_free(mesh);
        gf3d_texture_free(texture);
        return NULL;
    }
    model->mesh = mesh;
    model->texture = texture;
    gfc_line_cpy(model->filename,filename);
    return model;
}


Model *model_get_by_filename(const char *filename)
{
    int i;
    if (!filename)return NULL;
    for (i = 0; i < model_manager.modelCount;i++)
    {
        if (model_manager.modelList[i]._refCount == 0)continue;
        if (strlen(model_manager.modelList[i].filename) == 0)continue;
        if (gfc_strlcmp(model_manager.modelList[i].filename,filename) == 0)
        {
            return &model_manager.modelList[i];
        }
    }
    return NULL;
}

void model_free(Model *model)
{
    if(!model)return;
    model->_refCount--;
    //not deleting at 0 on purpose, model_new recycles unused slots and reloading is skipped if it gets asked for again
}

void model_delete(Model *model)
{
    if (!model)return;
    gf3d_texture_free(model->texture);
    gf3d_mesh_free(model->mesh);
    memset(model,0,sizeof(Model));
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
    if (view)gfc_matrix4_copy(ubo.view, *view);

    gf3d_vgraphics_get_projection_matrix(&ubo.proj);
    ubo.color = gfc_color_to_vector4f(colorMod);


    //slog("ubo color %f %f %f %f",ubo.color.x,ubo.color.y,ubo.color.z,ubo.color.w);


    return ubo;
}

void model_queue_render_sky(Model *model, GFC_Matrix4 modelMat, GFC_Color colorMod)
{
    ModelUBO ubo;
    if (!model)return;
    if (!model_manager.skyPipe)return;
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