#pragma once

/* ------------------------ ripped from tmc2------------------------ */
enum PCCPatchOrientation {
  PATCH_ORIENTATION_DEFAULT = 0,  // 0: default
  PATCH_ORIENTATION_SWAP    = 1,  // 1: swap
  PATCH_ORIENTATION_ROT90   = 2,  // 2: rotation 90
  PATCH_ORIENTATION_ROT180  = 3,  // 3: rotation 180
  PATCH_ORIENTATION_ROT270  = 4,  // 4: rotation 270
  PATCH_ORIENTATION_MIRROR  = 5,  // 5: mirror
  PATCH_ORIENTATION_MROT90  = 6,  // 6: mirror + rotation 90
  PATCH_ORIENTATION_MROT180 = 7,  // 7: mirror + rotation 180
  PATCH_ORIENTATION_MROT270 = 8   // 8: similar to SWAP, not used switched SWAP with ROT90 positions
};