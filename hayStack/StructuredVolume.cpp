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

/*! a hay-*stack* is a description of data-parallel data */

#include "hayStack/StructuredVolume.h"

namespace hs {

  box3f StructuredVolume::getBounds() const
  {
    box3f bb;
    bb.lower = gridOrigin;
    bb.upper = gridSpacing * vec3f(dims);
    return bb;
  }

  range1f StructuredVolume::getValueRange() const
  {
    int numScalars = dims.x*dims.y*dims.z;
    range1f range;
    switch (scalarType) {
    case FLOAT:
      for (int i=0;i<numScalars;i++) range.extend(((const float *)rawData.data())[i]);
      break;
    default:
      HAYSTACK_NYI();
    }
    return range;
  }
  
}
