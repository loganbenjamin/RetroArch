#include "../../../../file_extract.h"
#include "../../../../file_ops.h"
#include <file/file_path.h>

#include <stdio.h>
#include <string.h>
#include <retro_miscellaneous.h>
#include "../native/com_retroarch_browser_NativeInterface.h"

//#define VERBOSE_LOG

#ifndef VERBOSE_LOG
#undef RARCH_LOG
#define RARCH_LOG(...)
#endif

struct userdata
{
   const char *subdir;
   const char *dest;
};

static int zlib_cb(const char *name, const char *valid_exts,
      const uint8_t *cdata,
      unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   char path[PATH_MAX];
   char path_dir[PATH_MAX];
   struct userdata *user = userdata;
   const char *subdir = user->subdir;
   const char *dest   = user->dest;

   if (strstr(name, subdir) != name)
      return 1;

   name += strlen(subdir) + 1;

   fill_pathname_join(path, dest, name, sizeof(path));
   fill_pathname_basedir(path_dir, path, sizeof(path_dir));

   if (!path_mkdir(path_dir))
   {
      RARCH_ERR("Failed to create dir: %s.\n", path_dir);
      return 0;
   }

   RARCH_LOG("Extracting %s -> %s ...\n", name, path);

   switch (cmode)
   {
      case 0: /* Uncompressed */
         if (!write_file(path, cdata, size))
         {
            RARCH_ERR("Failed to write file: %s.\n", path);
            return 0;
         }
         break;

      case 8: /* Deflate */
         {
            int ret = 0;
            zlib_file_handle_t handle = {0};
            if (!zlib_inflate_data_to_file_init(&handle, cdata, csize, size))
               goto error;

            do{
               ret = zlib_inflate_data_to_file_iterate(handle.stream);
            }while(ret == 0);

            if (!zlib_inflate_data_to_file(&handle, ret, path, valid_exts,
                     cdata, csize, size, crc32))
               goto error;
         }
         break;

      default:
         return 0;
   }

   return 1;

error:
   RARCH_ERR("Failed to deflate to: %s.\n", path);
   return 0;
}

JNIEXPORT jboolean JNICALL Java_com_retroarch_browser_NativeInterface_extractArchiveTo(
      JNIEnv *env, jclass cls, jstring archive, jstring subdir, jstring dest)
{
   const char *archive_c = (*env)->GetStringUTFChars(env, archive, NULL);
   const char *subdir_c  = (*env)->GetStringUTFChars(env, subdir, NULL);
   const char *dest_c    = (*env)->GetStringUTFChars(env, dest, NULL);

   jboolean ret = JNI_TRUE;

   struct userdata data = {
      .subdir = subdir_c,
      .dest = dest_c,
   };

   if (!zlib_parse_file(archive_c, NULL, zlib_cb, &data))
   {
      RARCH_ERR("Failed to parse APK: %s.\n", archive_c);
      ret = JNI_FALSE;
   }

   (*env)->ReleaseStringUTFChars(env, archive, archive_c);
   (*env)->ReleaseStringUTFChars(env, subdir, subdir_c);
   (*env)->ReleaseStringUTFChars(env, dest, dest_c);
   return ret;
}
