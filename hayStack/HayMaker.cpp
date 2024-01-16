// ======================================================================== //
// Copyright 2022-2023 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "HayMaker.h"
#include <map>

namespace hs {

// #if HANARI
//   std::string g_libraryName = "barney";
//   static const bool g_true = true;
//   bool g_enableDebug = true;
//   static const char *g_traceDir = nullptr;
//   static anari::Library g_debug = nullptr;
//   bool g_verbose = true;
  
// static void statusFunc(const void *userData,
//     ANARIDevice device,
//     ANARIObject source,
//     ANARIDataType sourceType,
//     ANARIStatusSeverity severity,
//     ANARIStatusCode code,
//     const char *message)
// {
//   const bool verbose = userData ? *(const bool *)userData : false;
//   if (severity == ANARI_SEVERITY_FATAL_ERROR) {
//     fprintf(stderr, "[FATAL][%p] %s\n", source, message);
//     std::exit(1);
//   } else if (severity == ANARI_SEVERITY_ERROR)
//     fprintf(stderr, "[ERROR][%p] %s\n", source, message);
//   else if (severity == ANARI_SEVERITY_WARNING)
//     fprintf(stderr, "[WARN ][%p] %s\n", source, message);
//   else if (verbose && severity == ANARI_SEVERITY_PERFORMANCE_WARNING)
//     fprintf(stderr, "[PERF ][%p] %s\n", source, message);
//   else if (verbose && severity == ANARI_SEVERITY_INFO)
//     fprintf(stderr, "[INFO ][%p] %s\n", source, message);
//   else if (verbose && severity == ANARI_SEVERITY_DEBUG)
//     fprintf(stderr, "[DEBUG][%p] %s\n", source, message);
// }

//   static anari::Device initializeANARI()
// {
//   auto library =
//     anariLoadLibrary(g_libraryName.c_str(), statusFunc, &g_verbose);
//   if (!library)
//     throw std::runtime_error("Failed to load ANARI library");
  
//   if (g_enableDebug)
//     g_debug = anariLoadLibrary("debug", statusFunc, &g_true);
  
//   anari::Device dev = anariNewDevice(library, "default");
  
//   anari::unloadLibrary(library);
  
//   if (g_enableDebug)
//     anari::setParameter(dev, dev, "glDebug", true);
  
// #ifdef USE_GLES2
//   anari::setParameter(dev, dev, "glAPI", "OpenGL_ES");
// #else
//   anari::setParameter(dev, dev, "glAPI", "OpenGL");
// #endif

//   if (g_enableDebug) {
//     anari::Device dbg = anariNewDevice(g_debug, "debug");
//     anari::setParameter(dbg, dbg, "wrappedDevice", dev);
//     if (g_traceDir) {
//       anari::setParameter(dbg, dbg, "traceDir", g_traceDir);
//       anari::setParameter(dbg, dbg, "traceMode", "code");
//     }
//     anari::commitParameters(dbg, dbg);
//     anari::release(dev, dev);
//     dev = dbg;
//   }

//   anari::commitParameters(dev, dev);

//   return dev;
// }
// #endif

#if HANARI
  static void statusFunc(const void * /*userData*/,
    ANARIDevice /*device*/,
    ANARIObject source,
    ANARIDataType /*sourceType*/,
    ANARIStatusSeverity severity,
    ANARIStatusCode /*code*/,
    const char *message)
{
  if (severity == ANARI_SEVERITY_FATAL_ERROR) {
    fprintf(stderr, "[FATAL][%p] %s\n", source, message);
    std::exit(1);
  } else if (severity == ANARI_SEVERITY_ERROR) {
    fprintf(stderr, "[ERROR][%p] %s\n", source, message);
  } else if (severity == ANARI_SEVERITY_WARNING) {
    fprintf(stderr, "[WARN ][%p] %s\n", source, message);
  } else if (severity == ANARI_SEVERITY_PERFORMANCE_WARNING) {
    fprintf(stderr, "[PERF ][%p] %s\n", source, message);
  }
  // Ignore INFO/DEBUG messages
}
#endif
  
  HayMaker::HayMaker(Comm &world,
                     Comm &workers,
                     ThisRankData &thisRankData,
                     bool verbose)
    : world(world),
      workers(workers),
      rankData(std::move(thisRankData)),
      isActiveWorker(!thisRankData.empty()),
      verbose(verbose)
  {
    perDG.resize(rankData.size());
  }

  void HayMaker::createBarney()
  {
    if (isActiveWorker) {
      std::vector<int> dataGroupIDs;
      for (auto dg : rankData.dataGroups)
        dataGroupIDs.push_back(dg.dataGroupID);
#if HANARI
      
      auto library = anari::loadLibrary("barney", statusFunc);
      device = anari::newDevice(library, "default");
      anari::commitParameters(device, device);
#else
      barney = bnMPIContextCreate
        (world.comm,
         /*data*/dataGroupIDs.data(),dataGroupIDs.size(),
         /*gpus*/nullptr,-1);
#endif
    } else {
#if HANARI
      throw std::runtime_error("passive master not yet implemented");
#else
      barney = bnMPIContextCreate
        (world.comm,/*data*/nullptr,0,/*gpus*/nullptr,0);
#endif
    }
    
#if HANARI
    model = anari::newObject<anari::World>(device);
    anari::commitParameters(device, model);
    
    auto renderer = anari::newObject<anari::Renderer>(device, "default");
    anari::commitParameters(device, renderer);
    
    frame = anari::newObject<anari::Frame>(device);
    // anari::setParameter(device, frame, "size", imgSize);
    anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_RGBA_SRGB);
    anari::setParameter(device, frame, "world",    model);
    anari::setParameter(device, frame, "renderer", renderer);
    
    this->camera = anari::newObject<anari::Camera>(device, "perspective");
    
    anari::setParameter(device, frame, "camera",   camera);
    anari::commitParameters(device, frame);
#else
    fb = bnFrameBufferCreate(barney,0);
    model = bnModelCreate(barney);
#endif
  }
  
  BoundsData HayMaker::getWorldBounds() const
  {
    BoundsData bb = rankData.getBounds();
    bb.spatial.lower = world.allReduceMin(bb.spatial.lower);
    bb.spatial.upper = world.allReduceMax(bb.spatial.upper);
    bb.scalars.lower = world.allReduceMin(bb.scalars.lower);
    bb.scalars.upper = world.allReduceMax(bb.scalars.upper);
    return bb;
  }
  
  void HayMaker::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    this->fbSize = fbSize;
#if HANARI
    this->hostRGBA = hostRGBA;
    anari::setParameter(device, frame, "size", (const anari::math::uint2&)fbSize);
    anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_VEC4);
              
    anari::commitParameters(device, frame);
#else
    bnFrameBufferResize(fb,fbSize.x,fbSize.y,(world.rank==0)?hostRGBA:nullptr);
#endif
  }

  void HayMaker::setTransferFunction(const TransferFunction &xf) 
  {
    for (int dgID=0;dgID<rankData.size();dgID++) {
      auto &dg = perDG[dgID];
      if (dg.createdVolumes.empty())
        continue;

      for (auto vol : dg.createdVolumes) {
#if HANARI
        std::vector<anari::math::float3> colors;
        std::vector<float> opacities;

        std::cout << "should set real xf here ..." << std::endl;
        colors.emplace_back(0.f, 0.f, 1.f);
        colors.emplace_back(0.f, 1.f, 0.f);
        colors.emplace_back(1.f, 0.f, 0.f);

        opacities.emplace_back(0.f);
        opacities.emplace_back(1.f);

        anari::setAndReleaseParameter
          (device,
           vol,
           "color",
           anari::newArray1D(device, colors.data(), colors.size()));
        anari::setAndReleaseParameter
          (device,
           vol,
           "opacity",
           anari::newArray1D(device, opacities.data(), opacities.size()));
        // float voxelRange[2];
        // anariSetParameter(device, vol, "valueRange", ANARI_FLOAT32_BOX1, &g_voxelRange);
        range1f valueRange = xf.domain;
        anariSetParameter(device, vol, "valueRange", ANARI_FLOAT32_BOX1, &valueRange.lower);
        
        anari::commitParameters(device, vol);
        
        anari::setAndReleaseParameter
          (device, // m_state.
           model, "volume", anari::newArray1D(device, &vol));
        anari::release(device, vol);
#else
        bnVolumeSetXF(vol,
                      (float2&)xf.domain,
                      (const float4*)xf.colorMap.data(),
                      xf.colorMap.size(),
                      xf.baseDensity);
#endif
      }
    }
    parallel_for
      ((int)rankData.size(),
       [&](int dgID)
       {
         auto &dg = perDG[dgID];
         if (dg.createdVolumes.empty())
           return;
#if HANARI
         anari::commitParameters(device, dg.volumeGroup);
         anari::commitParameters(device, model);
#else
         bnGroupBuild(dg.volumeGroup);
         BNDataGroup barney = bnGetDataGroup(model,dgID);
         bnBuild(barney);
#endif
       });
  }
  
  void HayMaker::renderFrame()
  {
#if HANARI
    anari::commitParameters(device, frame);
    anari::render(device, frame);
    auto fb = anari::map<uint32_t>(device, frame, "channel.color");
    if (fb.width != fbSize.x || fb.height != fbSize.y)
      std::cout << "resized frame!?" << std::endl;
    else {
      if (hostRGBA)
        memcpy(hostRGBA,fb.data,fbSize.x*fbSize.y*sizeof(uint32_t));
    }
#else
    bnRender(model,&camera,fb);
#endif
  }

  void HayMaker::resetAccumulation()
  {
#if HANARI
#else
    bnAccumReset(fb);
#endif
  }

  void HayMaker::setCamera(const Camera &camera)
  {
#if HANARI
    anari::setParameter(device, this->camera, "aspect", fbSize.x / (float)fbSize.y);
    anari::setParameter(device, this->camera, "position", (const anari::math::float3&)camera.vp);
    vec3f camera_dir = camera.vi - camera.vp;
    anari::setParameter(device, this->camera, "direction", (const anari::math::float3&)camera_dir);
    anari::setParameter(device, this->camera, "up", (const anari::math::float3&)camera.vu);
    anari::commitParameters(device, this->camera); // commit each object to indicate modifications are done1
#else
    bnPinholeCamera(&this->camera,
                    (const float3&)camera.vp,
                    (const float3&)camera.vi,
                    (const float3&)camera.vu,
                    camera.fovy,
                    fbSize.x / float(fbSize.y));
#endif
  }
  
  void HayMaker::buildDataGroup(int dgID)
  {
#if HANARI
    
#else
    BNDataGroup barney = bnGetDataGroup(model,dgID);
#endif

    std::vector<vec4f> xfValues;
    for (int i=0;i<100;i++)
      xfValues.push_back(vec4f(.5f,.5f,0.5f,
                               clamp(5.f-fabsf(i-40),0.f,1.f)));

    auto &dg = perDG[dgID];
      
#if HANARI
    std::vector<anari::Surface>   rootGroupGeoms;
    std::vector<anari::Group>     groups;
#else
    std::vector<BNGeom>   rootGroupGeoms;
    std::vector<BNGroup>  groups;
#endif
    std::vector<affine3f> xfms;
    auto &myData = rankData.dataGroups[dgID];

    // ------------------------------------------------------------------
    // render all SphereGeoms
    // ------------------------------------------------------------------
    if (!myData.sphereSets.empty()) {
#if HANARI
#else
      std::vector<BNGeom>  &geoms = rootGroupGeoms;
      for (auto &sphereSet : myData.sphereSets) {
        BNMaterial material = BN_DEFAULT_MATERIAL;
        BNGeom geom
          = bnSpheresCreate(barney,
                            &material,
                            (float3*)sphereSet->origins.data(),
                            sphereSet->origins.size(),
                            sphereSet->radii.data(),
                            sphereSet->radius);
        geoms.push_back(geom);
      }
#endif
    }

    // ------------------------------------------------------------------
    // render all CylinderGeoms
    // ------------------------------------------------------------------
    if (!myData.cylinderSets.empty()) {
#if HANARI
#else
      std::vector<BNGeom>  &geoms = rootGroupGeoms;
      for (auto &cylinderSet : myData.cylinderSets) {
        BNMaterial material = BN_DEFAULT_MATERIAL;
        BNGeom geom
          = bnCylindersCreate(barney,
                              &material,
                              (float3*)cylinderSet->points.data(),
                              cylinderSet->points.size(),
                              (int2*)cylinderSet->indices.data(),
                              cylinderSet->indices.size(),
                              cylinderSet->radii.data(),
                              cylinderSet->radius);
        geoms.push_back(geom);
      }
#endif
    }

    // ------------------------------------------------------------------
    // render all (possibly instanced) triangle meshes from mini format
    // ------------------------------------------------------------------
    for (auto mini : myData.minis) {
      // anari::Geometry mesh = anariNewGeometry(device, "triangle");
      // anari::Surface surface = anariNewSurface(device);
      // anari::Group group = anariNewGroup(device);
      // anari::Instance instance = anariNewInstance(device, "transform");

      // anariSetParameter(device, surface, "geometry", ANARI_GEOMETRY, mesh);
      // anariSetParameter(device, surface, "material", ANARI_MATERIAL, material);
      // anariCommitParameters(device, surface);
// #if HANARI
// #else
// #endif

#if HANARI
      std::map<mini::Object::SP, anari::Group> miniGroups;
#else
      std::map<mini::Object::SP, BNGroup> miniGroups;
#endif
      for (auto inst : mini->instances) {
        if (!miniGroups[inst->object]) {
#if HANARI
          std::vector<anari::Surface> geoms;
#else
          std::vector<BNGeom> geoms;
#endif
          for (auto miniMesh : inst->object->meshes) {
#if HANARI
            anari::Material material
              = anari::newObject<anari::Material>(device, "matte");
            anari::math::float3 color(miniMesh->material->baseColor.x,
                                      miniMesh->material->baseColor.y,
                                      miniMesh->material->baseColor.z// ,
                                      // 1.f
                                      );
            anari::setParameter(device,material,"color",color);
            anari::commitParameters(device, material);
            
            anari::Geometry mesh
              = anari::newObject<anari::Geometry>(device, "triangle");
            anari::setParameterArray1D(device, mesh, "vertex.position",
                                       (const anari::math::float3*)miniMesh->vertices.data(),
                                       miniMesh->vertices.size());
            // anari::setParameterArray1D(device, mesh, "vertex.color", color, 4);
            anari::setParameterArray1D(device, mesh, "primitive.index",
                                       (const anari::math::uint3*)miniMesh->indices.data(),
                                       miniMesh->indices.size());
            anari::commitParameters(device, mesh);
            
            anari::Surface  surface = anari::newObject<anari::Surface>(device);
            anari::setAndReleaseParameter(device, surface, "geometry", mesh);
            anari::setAndReleaseParameter(device, surface, "material", material);
            anari::commitParameters(device, surface);
            
            auto geom = surface;
#else
            BNMaterial material = BN_DEFAULT_MATERIAL;
            BNGeom geom
              = bnTriangleMeshCreate
              (barney,&material,
               (int3*)miniMesh->indices.data(),miniMesh->indices.size(),
               (float3*)miniMesh->vertices.data(),miniMesh->vertices.size(),
               nullptr,nullptr);
#endif
            geoms.push_back(geom);
          }
#if HANARI
          anari::Group meshGroup
            = anari::newObject<anari::Group>(device);
          anari::setParameterArray1D(device, meshGroup, "surface", geoms.data(),geoms.size());
          anari::commitParameters(device, meshGroup);
#else
          BNGroup meshGroup
            = bnGroupCreate(barney,geoms.data(),geoms.size(),
                            nullptr,0);
          bnGroupBuild(meshGroup);
#endif
          miniGroups[inst->object] = meshGroup;
        }
        groups.push_back(miniGroups[inst->object]);
        xfms.push_back((const affine3f&)inst->xfm);
      }
    }
    
    // ------------------------------------------------------------------
    // render all UMeshes
    // ------------------------------------------------------------------
    for (auto unst : myData.unsts) {
#if HANARI
#else
      BNMaterial material = BN_DEFAULT_MATERIAL;
      std::vector<vec4f> verts4f;
      std::vector<int> gridOffsets;
      std::vector<vec3i> gridDims;
      std::vector<box4f> gridDomains;
      const std::vector<float> &gridScalars = unst->gridScalars;
      const float *scalars = unst->perVertex->values.data();

      for (int i=0;i<unst->grids.size();i++) {
        const umesh::Grid &grid = unst->grids[i];
        gridDims.push_back((vec3i &)grid.numCells);
        gridDomains.push_back((box4f &)grid.domain);
        gridOffsets.push_back(grid.scalarsOffset);
      }

      for (int i=0;i<unst->vertices.size();i++) {
        verts4f.push_back(vec4f(unst->vertices[i].x,
                                unst->vertices[i].y,
                                unst->vertices[i].z,
                                scalars[i]));
      }

      BNScalarField mesh = bnUMeshCreate
        (barney,
         // vertices: 4 floats each
         (const float *)verts4f.data(),verts4f.size(),
         // tets: 4 ints each
         (const int *)unst->tets.data(),(int)unst->tets.size(),
         (const int *)unst->pyrs.data(),(int)unst->pyrs.size(),
         (const int *)unst->wedges.data(),(int)unst->wedges.size(),
         (const int *)unst->hexes.data(),(int)unst->hexes.size(),
         (int)unst->grids.size(),
         gridOffsets.data(),
         (const int *)gridDims.data(),
         (const float *)gridDomains.data(),
         gridScalars.data(), (int)gridScalars.size());
      // rootGroupGeoms.push_back(geom);
      BNVolume volume = bnVolumeCreate(barney,mesh);
      dg.createdVolumes.push_back(volume);
#endif
    }
    

    // ------------------------------------------------------------------
    // render all UMeshes
    // ------------------------------------------------------------------
    for (auto vol : myData.structuredVolumes) {
#if HANARI
#else
      BNScalarType scalarType;
      switch(vol->scalarType) {
      case StructuredVolume::UINT8:
        scalarType = BN_UINT8;
        break;
      case StructuredVolume::FLOAT:
        scalarType = BN_FLOAT;
        break;
      default: throw std::runtime_error("Unknown scalar type");
      }
      BNMaterial material = BN_DEFAULT_MATERIAL;
      PING; PRINT(vol->dims);
      BNScalarField bnVol = bnStructuredDataCreate
        (barney,
         (const int3&)vol->dims,
         scalarType,
         vol->rawData.data(),
         (const float3&)vol->gridOrigin,
         (const float3&)vol->gridSpacing);
      dg.createdVolumes.push_back(bnVolumeCreate(barney,bnVol));
#endif
    }
    

    // ------------------------------------------------------------------
    // create a single instance for all 'root' geometry that isn't
    // explicitly instantitated itself
    // ------------------------------------------------------------------
    if (!rootGroupGeoms.empty()) {
#if HANARI
      anari::Group rootGroup
        = anariNewGroup(device);
#else
      BNGroup rootGroup
        = bnGroupCreate(barney,rootGroupGeoms.data(),rootGroupGeoms.size(),
                        nullptr,0);
      bnGroupBuild(rootGroup);
      groups.push_back(rootGroup);
      xfms.push_back(affine3f());
#endif
    }
    // ------------------------------------------------------------------
    // create a single instance for all 'root' volume(s)
    // ------------------------------------------------------------------
    if (!dg.createdVolumes.empty()) {
#if HANARI
#else
      BNGroup rootGroup
        = bnGroupCreate(barney,nullptr,0,
                        dg.createdVolumes.data(),
                        dg.createdVolumes.size());
      bnGroupBuild(rootGroup);
      groups.push_back(rootGroup);
      xfms.push_back(affine3f());
      dg.volumeGroup = rootGroup;
#endif
    }

      
    // ------------------------------------------------------------------
    // finally - specify top-level instances for all the stuff we
    // generated
    // -----------------------------------------------------------------
#if HANARI

    std::vector<anari::Instance> instances;
    for (int i=0;i<groups.size();i++) {
      anari::Instance inst = anari::newObject<anari::Instance>(device,"transform");
      anari::setParameter(device, inst, "group", groups[i]);
      // anari::setAndReleaseParameter(device, inst, "group", groups[i]);

      const affine3f xfm = xfms[i];
      
      anari::math::mat4 axf = anari::math::identity;
      axf[0].x = xfm.l.vx.x;
      axf[0].y = xfm.l.vx.y;
      axf[0].z = xfm.l.vx.z;
      axf[1].x = xfm.l.vy.x;
      axf[1].y = xfm.l.vy.y;
      axf[1].z = xfm.l.vy.z;
      axf[2].x = xfm.l.vz.x;
      axf[2].y = xfm.l.vz.y;
      axf[2].z = xfm.l.vz.z;
      axf[3].x = xfm.p.x;
      axf[3].y = xfm.p.y;
      axf[3].z = xfm.p.z;
      anari::setParameter(device, inst, "transform", axf);
      // // getParam("transform", ANARI_FLOAT32_MAT4, &xfm);
      // m_xfm.xfm.l.vx = make_float3(xfm[0].x, xfm[0].y, xfm[0].z);
      // m_xfm.xfm.l.vy = make_float3(xfm[1].x, xfm[1].y, xfm[1].z);
      // m_xfm.xfm.l.vz = make_float3(xfm[2].x, xfm[2].y, xfm[2].z);
      // m_xfm.xfm.p = make_float3(xfm[3].x, xfm[3].y, xfm[3].z);
      
      // anari::setAndReleaseParameter(device, surface, "material", material);
      // todo: transform
      // anariSetParameter(self.device, instance, 'transform', ANARI_FLOAT32_MAT4, transform)
      anari::commitParameters(device, inst);
      instances.push_back(inst);
    }
    anari::setAndReleaseParameter
      (device,
       model,
       "instance",
       anari::newArray1D(device, instances.data(),instances.size()));
    anari::commitParameters(device, model);
#else
    
    bnSetInstances(barney,groups.data(),(BNTransform *)xfms.data(),
                        groups.size());
    bnBuild(barney);
#endif
  }
  
}
