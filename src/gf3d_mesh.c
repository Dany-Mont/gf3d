#include "simple_logger.h"

#include "gfc_types.h"

#include "gf3d_buffers.h"
#include "gf3d_swapchain.h"
#include "gf3d_vgraphics.h"
#include "gf3d_pipeline.h"
#include "gf3d_commands.h"
#include "gf3d_mesh.h"
#include "gf3d_obj_load.h"


typedef struct
{
    Uint32          meshCount;
    Mesh            *meshList;
    VkDevice        device;
    VkVertexInputAttributeDescription attributeDescriptions[MESH_ATTRIBUTE_COUNT];
    VkVertexInputBindingDescription bindingDescription;
}MeshManager;

static MeshManager mesh_manager = {0};

//forward declarations start
void gf3d_mesh_system_close();
void gf3d_mesh_delete(Mesh *mesh);
void gf3d_mesh_primitive_free(MeshPrimitive *prim);
int gf3d_mesh_primitive_buffer_create(MeshPrimitive *prim);
void gf3d_mesh_primitive_buffer_free(MeshPrimitive *prim);
Mesh *gf3d_mesh_get_by_filename(const char *filename);
//forward declarations end


void gf3d_mesh_system_init(Uint32 mesh_max)
{
    if (mesh_manager.meshCount != 0)
    {
        slog("mesh system already initialized");
        return;
    }
    if (mesh_max == 0)
    {
        slog("cannot initialize mesh system with 0 meshes");
        return;
    }
    mesh_manager.meshList = gfc_allocate_array(sizeof(Mesh),mesh_max);
    if (!mesh_manager.meshList)
    {
        //gfc_allocate_array already logged the error
        return;
    }
    mesh_manager.meshCount = mesh_max;
    mesh_manager.device = gf3d_vgraphics_get_default_logical_device();
    //the attribute and binding descriptions are filled in by their getters,
    //model_manager_init calls those when it builds the pipeline
    atexit(gf3d_mesh_system_close);
}

void gf3d_mesh_system_close()
{
    int i;

    //go through mesh list and free all
    for (i = 0; i < mesh_manager.meshCount;i++)
    {
        gf3d_mesh_delete(&mesh_manager.meshList[i]);
    }
    free(mesh_manager.meshList);
    memset(&mesh_manager,0,sizeof(MeshManager));
}


Mesh *gf3d_mesh_new()
{
    int i;
    int emptyIndex = -1;
    for (i = 0; i < mesh_manager.meshCount;i++)
    {
        if (mesh_manager.meshList[i]._refCount != 0)continue;
        if (emptyIndex < 0)emptyIndex = i;
        if (strlen(mesh_manager.meshList[i].filename) == 0)
        {
            //never used slot, take it
            mesh_manager.meshList[i].primitives = gfc_list_new();
            if (mesh_manager.meshList[i].primitives == NULL)
            {
                slog("cannot allocate more memory for a new mesh");
                return NULL;
            }
            mesh_manager.meshList[i]._refCount = 1;
            return &mesh_manager.meshList[i];
        }
    }
    if (emptyIndex < 0)
    {
        slog("gf3d_mesh_new: no free slots for new meshes");
        return NULL;
    }
    //every slot has been used once, recycle the first one nobody is using
    gf3d_mesh_delete(&mesh_manager.meshList[emptyIndex]);
    mesh_manager.meshList[emptyIndex].primitives = gfc_list_new();
    if (mesh_manager.meshList[emptyIndex].primitives == NULL)
    {
        slog("cannot allocate more memory for a new mesh");
        return NULL;
    }
    mesh_manager.meshList[emptyIndex]._refCount = 1;
    return &mesh_manager.meshList[emptyIndex];
}

void gf3d_mesh_free(Mesh *mesh)
{
    if(!mesh)return;
    if (mesh->_refCount > 0)mesh->_refCount--;
    if (mesh->_refCount > 0)return;
    gf3d_mesh_delete(mesh);
}

void gf3d_mesh_delete(Mesh *mesh)
{
    int i,c;
    MeshPrimitive *prim;
    if(!mesh)return;
    if (mesh->primitives)
    {
        c = gfc_list_count(mesh->primitives);
        for(i=0;i<c;i++)
        {
            prim = gfc_list_nth(mesh->primitives,i);
            if(!prim)continue;
            gf3d_mesh_primitive_free(prim);
        }
        gfc_list_delete(mesh->primitives);
    }
    memset(mesh,0,sizeof(Mesh));
}


void gf3d_mesh_primitive_free(MeshPrimitive *prim)
{
    if(!prim)return;
    gf3d_mesh_primitive_buffer_free(prim);
    if (prim->objData)
    {
        gf3d_obj_free(prim->objData);
    }
    free(prim);
}

void gf3d_mesh_primitive_buffer_free(MeshPrimitive *prim)
{
    if(!prim)return;
    if (prim->faceBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mesh_manager.device,prim->faceBuffer,NULL);
        prim->faceBuffer = VK_NULL_HANDLE;
    }
    if (prim->faceBufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(mesh_manager.device,prim->faceBufferMemory,NULL);
        prim->faceBufferMemory = VK_NULL_HANDLE;
    }
    if (prim->vertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mesh_manager.device,prim->vertexBuffer,NULL);
        prim->vertexBuffer = VK_NULL_HANDLE;
    }
    if (prim->vertexBufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(mesh_manager.device,prim->vertexBufferMemory,NULL);
        prim->vertexBufferMemory = VK_NULL_HANDLE;
    }
}

Mesh *gf3d_mesh_get_by_filename(const char *filename)
{
    int i;
    if (!filename)return NULL;
    for (i = 0; i < mesh_manager.meshCount;i++)
    {
        if (mesh_manager.meshList[i]._refCount == 0)continue;
        if (strlen(mesh_manager.meshList[i].filename) == 0)continue;
        if (gfc_strlcmp(mesh_manager.meshList[i].filename,filename) == 0)
        {
            return &mesh_manager.meshList[i];
        }
    }
    return NULL;
}

/**
 * @brief load mesh data from an obj filename.
 * @note: currently only supporting obj files
 * @note this free's the intermediate data loaded from the obj file, no longer needed for most applications
 * @param filename the name of the file to load
 * @return NULL on error or Mesh data
 */
Mesh *gf3d_mesh_load_obj(const char *filename)
{
    Mesh *mesh;
    MeshPrimitive *primitive;

    if (!filename)return NULL;
    mesh = gf3d_mesh_get_by_filename(filename);
    if (mesh)
    {
        mesh->_refCount++;
        return mesh;
    }

    mesh = gf3d_mesh_new();
    if (!mesh)
    {
        slog("failed to allocate a new mesh for %s",filename);
        return NULL;
    }
    primitive = gf3d_mesh_primitive_new();
    if (!primitive)
    {
        slog("failed to get new primitive for mesh %s",filename);
        gf3d_mesh_delete(mesh);
        return NULL;
    }
    primitive->objData = gf3d_obj_load_from_file(filename);
    if (!primitive->objData)
    {
        slog("failed to parse file %s for obj data",filename);
        gf3d_mesh_primitive_free(primitive);
        gf3d_mesh_delete(mesh);
        return NULL;
    }
    if (!gf3d_mesh_primitive_buffer_create(primitive))
    {
        slog("failed to build memory buffers for %s",filename);
        gf3d_mesh_primitive_free(primitive);
        gf3d_mesh_delete(mesh);
        return NULL;
    }
    gfc_list_append(mesh->primitives,primitive);
    gfc_line_cpy(mesh->filename,filename);
    return mesh;
}

MeshPrimitive *gf3d_mesh_primitive_new()
{
    MeshPrimitive *prim;
    prim = gfc_allocate_array(sizeof(MeshPrimitive),1);
    if(!prim)
    {
        //gfc_allocate_array already logged the error
        return NULL;
    }
    return prim;
}

/**
 * @brief get the input attribute descriptions for mesh based rendering
 * @param count (optional, output) the number of attributes
 * @return a pointer to a vertex input attribute description array
 */
VkVertexInputAttributeDescription * gf3d_mesh_get_attribute_descriptions(Uint32 *count)
{
    if(count)*count = MESH_ATTRIBUTE_COUNT;
    mesh_manager.attributeDescriptions[0].binding = 0;
    mesh_manager.attributeDescriptions[0].location = 0;
    mesh_manager.attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    mesh_manager.attributeDescriptions[0].offset = offsetof(Vertex, vertex);

    mesh_manager.attributeDescriptions[1].binding = 0;
    mesh_manager.attributeDescriptions[1].location = 1;
    mesh_manager.attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    mesh_manager.attributeDescriptions[1].offset = offsetof(Vertex, normal);

    mesh_manager.attributeDescriptions[2].binding = 0;
    mesh_manager.attributeDescriptions[2].location = 2;
    mesh_manager.attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    mesh_manager.attributeDescriptions[2].offset = offsetof(Vertex, texel);
    return mesh_manager.attributeDescriptions;
}

/**
 * @brief get the binding description for mesh based rendering
 * @return vertex input binding descriptions compatible with mesh data
 */
VkVertexInputBindingDescription * gf3d_mesh_get_bind_description()
{
    mesh_manager.bindingDescription.binding = 0;
    mesh_manager.bindingDescription.stride = sizeof(Vertex);
    mesh_manager.bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return &mesh_manager.bindingDescription;
}


void gf3d_mesh_queue_render(Mesh *mesh,Pipeline *pipe,void *uboData,Texture *texture)
{
    int c,i;
    MeshPrimitive *prim;

    if (!mesh)
    {
        slog("cannot render a mesh that doesn't exist");
        return;
    }
    if (!pipe)
    {
        slog("cannot render a mesh with a pipeline that doesn't exist");
        return;
    }
    if (!uboData)
    {
        slog("cannot render a mesh without uboData");
        return;
    }
    if (!texture)
    {
        slog("cannot render a mesh without a texture");
        return;
    }
    c = gfc_list_count(mesh->primitives);
    for(i = 0; i < c; i++)
    {
        prim = gfc_list_nth(mesh->primitives,i);
        if(!prim)continue;
        gf3d_pipeline_queue_render(pipe,prim->vertexBuffer,prim->vertexCount,prim->faceBuffer,uboData,texture);
    }
}

/**
 * @brief create a mesh primitive's gpu buffers from its objData
 * @param prim the mesh primitive to populate, objData must already be set
 * @return 1 on success, 0 on failure
 */
int gf3d_mesh_primitive_buffer_create(MeshPrimitive *prim)
{
    void *data = NULL;
    size_t bufferSize;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    if ((!prim)||(!prim->objData))
    {
        slog("cannot create primitive buffers for a non-existent mesh primitive");
        return 0;
    }

    //face (index) buffer
    bufferSize = sizeof(Face) * prim->objData->face_count;
    gf3d_buffer_create(bufferSize,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&stagingBuffer,&stagingBufferMemory);
    vkMapMemory(mesh_manager.device,stagingBufferMemory,0,bufferSize,0,&data);
    memcpy(data,prim->objData->outFace,bufferSize);
    vkUnmapMemory(mesh_manager.device,stagingBufferMemory);
    gf3d_buffer_create(bufferSize,VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&prim->faceBuffer,&prim->faceBufferMemory);
    gf3d_buffer_copy(stagingBuffer,prim->faceBuffer,bufferSize);
    vkDestroyBuffer(mesh_manager.device,stagingBuffer,NULL);
    vkFreeMemory(mesh_manager.device,stagingBufferMemory,NULL);
    prim->faceCount = prim->objData->face_count;

    //vertex buffer
    bufferSize = sizeof(Vertex) * prim->objData->face_vert_count;
    gf3d_buffer_create(bufferSize,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&stagingBuffer,&stagingBufferMemory);
    vkMapMemory(mesh_manager.device,stagingBufferMemory,0,bufferSize,0,&data);
    memcpy(data,prim->objData->faceVertices,bufferSize);
    vkUnmapMemory(mesh_manager.device,stagingBufferMemory);
    gf3d_buffer_create(bufferSize,VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&prim->vertexBuffer,&prim->vertexBufferMemory);
    gf3d_buffer_copy(stagingBuffer,prim->vertexBuffer,bufferSize);
    vkDestroyBuffer(mesh_manager.device,stagingBuffer,NULL);
    vkFreeMemory(mesh_manager.device,stagingBufferMemory,NULL);
    prim->vertexCount = prim->objData->face_vert_count;

   slog("mesh buffers: faces %i, face verts %i",prim->faceCount,prim->vertexCount);

    return 1;
}
