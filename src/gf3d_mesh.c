#include "simple_json.h"
#include "gf2d_sprite.h"

#include "gf3d_vgraphics.h"
#include "gf3d_buffers.h"
#include "gf3d_obj_load.h"


#include "gf3d_mesh.h"


typedef struct 
{
    Uint32          meshCount;
    Mesh            *meshList;
    VkDevice      device;
    VkVertexInputAttributeDescription attributeDescriptions[3];
    VkVertexInputBindingDescription bindingDescription;
}MeshManager;

static MeshManager mesh_manager = {0};

void gf3d_mesh_close();
void gf3d_mesh_free(Mesh *mesh);


void gf3d_mesh_init(Uint32 mesh_max)
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
    atexit(gf3d_mesh_close);
}

void gf3d_mesh_close()
{
    int i;
    
    //go through mesh list and free all
    for (i = 0; i < mesh_manager.meshCount;i++)
    {
        gf3d_mesh_free(&mesh_manager.meshList[i]);
    }
    free(mesh_manager.meshList);
    memset(&mesh_manager,0,sizeof(MeshManager));
}


Mesh *gf3d_mesh_new()
{
    int i;
    for (i = 0; i < mesh_manager.meshCount;i++)
    {
        if (mesh_manager.meshList[i]._refCount == 0)
        {
            mesh_manager.meshList[i]._refCount = 1;
            mesh_manager.meshList[i].primitives = gfc_list_new();
            mesh_manager.device = VKDevice();
            if (mesh_manager.meshList[i].primitives == NULL)
            {
                slog("cannot allocate more memory for a new mesh");
                return NULL;
             };
        }
    }
    return NULL;
}

void gf3d_mesh_free(Mesh *mesh)
{
    int i,c;
    MeshPrimitive *prim;
    if(!mesh)return;
    c = gfc_list_count(mesh->primitives);
    for(i=0;i<c;i++)
    {
        prim = gfc_list_nth(mesh->primitives,i);
        if(!prim)continue;
        gf3d_mesh_primitive_free(prim);
    }
}


void gf3d_mesh_primitive_free(MeshPrimitive *prim)
{
    if(!prim)return;


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

    ObjData* obj = NULL;
    mesh = gf3d_mesh_get_by_filename(filename);
    if (mesh)return mesh;
    
    obj = gf3d_obj_load_from_file(filename);
    
    if (!obj)
    {
        return NULL;
    }
    
    mesh = gf3d_mesh_new();
    if (!mesh)
    {
        return NULL;
    }
    gfc_line_cpy(mesh->filename,filename);
    primitive->objData = obj;
    gf3d_mesh_create_vertex_buffer_from_vertices(primitive);

    gfc_list_append(mesh->primitives,primitive);

    
    return mesh;
}
MeshPrimitive *gf3d_mesh_primitive_new()
{
    MeshPrimitive *prim;
    prim = gfc_allocate_array(sizeof(MeshPrimitive),1);
    if(!prim)
    {
        slog("prim could not be created");
        return 0;
    }
    return prim;
}

/**
 * @brief make an exact, but separate copy of the input mesh
 * @param in the mesh to duplicate
 * @return NULL on error, or a copy of in
 */
Mesh *gf3d_mesh_copy(Mesh *in);

/**
 * @brief move all of the vertices of the mesh by offset at the buffer level
 * @param in the mesh to move
 * @param offset how much to move it
 * @param rotation apply this rotation to the vertices and normals
 */
void gf3d_mesh_move_vertices(Mesh *in, GFC_Vector3D offset,GFC_Vector3D rotation);

/**
 * @brief allocate a zero initialized mesh primitive
 * @return NULL on error or the primitive
 */
MeshPrimitive *gf3d_mesh_primitive_new();


/**
 * @brief get the input attribute descriptions for mesh based rendering
 * @param count (optional, output) the number of attributes
 * @return a pointer to a vertex input attribute description array
 */
VkVertexInputAttributeDescription * gf3d_mesh_get_attribute_descriptions(Uint32 *count)
{
    if(count)*count = 3;
    mesh_manager.attributeDescriptions[0].binding = 0;
    mesh_manager.attributeDescriptions[0].location = 0;
    mesh_manager.attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    mesh_manager.attributeDescriptions[0].offset = offsetof(Vertex, vertex);
   
    mesh_manager.attributeDescriptions[1].binding = 0;
    mesh_manager.attributeDescriptions[1].location = 1;
    mesh_manager.attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    mesh_manager.attributeDescriptions[1].offset = offsetof(Vertex, normal);

    mesh_manager.attributeDescriptions[2].binding = 0;
    mesh_manager.attributeDescriptions[2].location = 1;
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


/**
 * @brief needs to be called once at the beginning of each render frame
 */
void gf3d_mesh_reset_pipes();

/**
 * @brief called to submit all draw commands to the mesh pipelines
 */
void gf3d_mesh_submit_pipe_commands();

/**
 * @brief get the current command buffer for the mesh system
 */
VkCommandBuffer gf3d_mesh_get_model_command_buffer();


void gf3d_mesh_queue_render(Mesh *mesh,Pipeline *pipe,void *uboData,Texture *texture)
{
    int c,i;
    MeshPrimitive *prim;
    
    if (!mesh)return;
    if (!pipe)return;
    if (!uboData)return;
    if (!texture)return;
    c = gfc_list_count(mesh->primitives);
    for(i = 0; i < c; i++)
    {
      prim = gfc_list_nth(mesh->primitives,i);
      if(!prim)continue;
      gf3d_pipeline_queue_render(pipe,prim->vertexBuffer,prim->vertexCount,prim->faceBuffer,uboData,texture);
    }
    
    gf3d_pipeline_queue_render(pipe,prim->vertexBuffer,prim->vertexCount,prim->faceBuffer,uboData,texture);
}


/**
 * @brief adds a mesh to the render pass rendered as an outline highlight
 * @note: must be called within the render pass
 * @param mesh the mesh to render
 * @param com the command pool to use to handle the request we are rendering with
 */
void gf3d_mesh_render(Mesh *mesh,VkCommandBuffer commandBuffer, VkDescriptorSet * descriptorSet);

/**
 * @brief render a mesh through a given pipeline
 */
void gf3d_mesh_render_generic(Mesh *mesh,Pipeline *pipe,VkDescriptorSet * descriptorSet);

/**
 * @brief create a mesh's internal buffers based on vertices
 * @param primitive the mesh primitive to populate
 * @note the primitive must have the objData set and it must have be organizes in buffer order
 */
void gf3d_mesh_create_vertex_buffer_from_vertices(MeshPrimitive *primitive);

/**
 * @brief get the pipeline that is used to render basic 3d meshes
 * @return NULL on error or the pipeline in question
 */
Pipeline *gf3d_mesh_get_pipeline();

/**
 * @brief given a model matrix and basic color, build the meshUBO needed to render a model
 * @param modelMat the model Matrix
 * @param colorMod the color for the UBO
 */
gf3d_mesh_primitive_buffer_create(MeshPrimitive *prim)
{
    Uint32 bufferSize = 0;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    if(!prim)return 0;
    bufferSize = sizeof(Vertex) * prim->vertexCount;
    gf3d_vgraphics_create_buffer(bufferSize,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&stagingBuffer,&stagingBufferMemory);
    gf3d_vgraphics_copy_to_buffer(stagingBuffer,prim->objData->vertices,bufferSize);
    gf3d_vgraphics_create_buffer(bufferSize,VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&prim->vertexBuffer,&prim->vertexBufferMemory);
    gf3d_vgraphics_copy_buffer(stagingBuffer,prim->vertexBuffer,bufferSize);
    vkDestroyBuffer(mesh_manager.device,stagingBuffer,NULL);
    vkFreeMemory(mesh_manager.device,stagingBufferMemory,NULL);
    bufferSize = sizeof(Uint32) * prim->faceCount;
    gf3d_vgraphics_create_buffer(bufferSize,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&stagingBuffer,&stagingBufferMemory);
    gf3d_vgraphics_copy_to_buffer(stagingBuffer,prim->objData->outFace,bufferSize);
    gf3d_vgraphics_create_buffer(bufferSize,VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&prim->faceBuffer,&prim->faceBufferMemory);
    gf3d_vgraphics_copy_buffer(stagingBuffer,prim->faceBuffer,bufferSize);
    vkDestroyBuffer(mesh_manager.device,stagingBuffer,NULL);
    vkFreeMemory(mesh_manager.device,stagingBufferMemory,NULL);
}
gf3d_mesh_buffer_create(Mesh *mesh)
{
    int i,c;
    MeshPrimitive *prim;
    if (!mesh)return 0;
    prim = gf3d_mesh_primitive_buffer_new();
    if (!prim)
    {
        slog ("failed to get new primitive for the mesh");
        return 0;
    }
    if(gf3d_mesh_primitive_buffer_create(prim))
    {
        gfc_list_append(mesh->primitives,prim);
    }
}

MeshUBO gf3d_mesh_get_ubo(
    GFC_Matrix4 modelMat,
    GFC_Color colorMod);


MeshPrimitive gf3d_mesh_primitive_buffer_new()
{
    MeshPrimitive *prim;
    prim = gfc_allocate_array(sizeof(MeshPrimitive),1);
    if(!prim)
    {
        slog("prim could not be created");
        return NULL;
    }
    return *prim;
}

