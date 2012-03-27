/**
*
* @file      sound/backends_parameters.h
* @brief     Backends parameters names
* @version   $Id$
* @author    (C) Vitamin/CAIG/2001
*
**/

#pragma once
#ifndef __SOUND_BACKENDS_PARAMETERS_H_DEFINED__
#define __SOUND_BACKENDS_PARAMETERS_H_DEFINED__

//common includes
#include <parameters.h>

namespace Parameters
{
  namespace ZXTune
  {
    namespace Sound
    {
      //! @brief Backend-specific parameters namespace
      namespace Backends
      {
        //! @brief Parameters#ZXTune#Sound#Backends namespace prefix
        const Char PREFIX[] =
        {
          'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','\0'
        };

        //! @brief Any file-based backend parameters namespace
        namespace File
        {
          //! @brief Short parameters names
          const Char FILENAME_PARAMETER[] =
          {
            'f','i','l','e','n','a','m','e','\0'
          };

          const Char OVERWRITE_PARAMETER[] =
          {
            'o','v','e','r','w','r','i','t','e','\0'
          };

          const Char BUFFERS_PARAMETER[] =
          {
            'b','u','f','f','e','r','s','\0'
          };

          //@{
          //! @name Any file-based backend basic parameters

          //! @brief Output filename template
          //! @see core/module_attrs.h for possibly supported field names
          const Char FILENAME[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','f','i','l','e','.','f','i','l','e','n','a','m','e','\0'
          };

          //! @brief Rewrite output files if exist
          //! @note Not zero if rewrite
          const Char OVERWRITE[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','f','i','l','e','.','o','v','e','r','w','r','i','t','e','\0'
          };

          //! @brief Buffers count to asynchronous saving
          //! @note Not zero if use asynchronous saving
          const Char BUFFERS[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','f','i','l','e','.','b','u','f','f','e','r','s','\0'
          };
          //@}
        }

        //! @brief %Win32 backend parameters namespace
        namespace Win32
        {
          //@{
          //! @name Win32 backend parameters

          //! Default value
          const IntType DEVICE_DEFAULT = -1;
          //! 0-based output device index
          const Char DEVICE[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','w','i','n','3','2','.','d','e','v','i','c','e','\0'
          };

          //! Default value
          const IntType BUFFERS_DEFAULT = 3;
          //! Buffers count
          const Char BUFFERS[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','w','i','n','3','2','.','b','u','f','f','e','r','s','\0'
          };
          //@}
        }

        //! @brief %OSS backend parameters
        namespace OSS
        {
          //@{
          //! @name OSS backend parameters

          //! Default value
          const Char DEVICE_DEFAULT[] = {'/', 'd', 'e', 'v', '/', 'd', 's', 'p', '\0'};
          //! Device filename
          const Char DEVICE[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','o','s','s','.','d','e','v','i','c','e','\0'
          };

          //! Default value
          const Char MIXER_DEFAULT[] = {'/', 'd', 'e', 'v', '/', 'm', 'i', 'x', 'e', 'r', '\0'};
          //! Mixer filename
          const Char MIXER[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','o','s','s','.','m','i','x','e','r','\0'
          };
          //@}
        }

        //! @brief %ALSA backend parameters
        namespace ALSA
        {
          //@{
          //! @name ALSA backend parameters

          //! Default value
          const Char DEVICE_DEFAULT[] = {'d', 'e', 'f', 'a', 'u', 'l', 't', '\0'};
          //! Device name
          const Char DEVICE[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','a','l','s','a','.','d','e','v','i','c','e','\0'
          };

          //! Mixer name
          const Char MIXER[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','a','l','s','a','.','m','i','x','e','r','\0'
          };

          //! Default value
          const IntType BUFFERS_DEFAULT = 2;
          //! Buffers count
          const Char BUFFERS[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','a','l','s','a','.','b','u','f','f','e','r','s','\0'
          };
          //@}
        }

        //! @brief %SDL backend parameters
        namespace SDL
        {
          //@{
          //! @name SDL backend parameters

          //! Default value
          const IntType BUFFERS_DEFAULT = 5;
          //! Buffers count
          const Char BUFFERS[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','s','d','l','.','b','u','f','f','e','r','s','\0'
          };
          //@}
        }

        //! @brief %DirectSound backend parameters
        namespace DirectSound
        {
          //@{
          //! @name DirectSound backend parameters

          //! Default value
          const IntType LATENCY_DEFAULT = 100;
          //! Buffers count
          const Char LATENCY[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','d','s','o','u','n','d','.','l','a','t','e','n','c','y','\0'
          };
          //@}
        }

        //! @brief %Mp3 backend parameters
        namespace Mp3
        {
          //@{
          //! @name Mp3 backend parameters

          //! Constant bitrate in kbps. Highest priority
          const Char BITRATE[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','m','p','3','.','b','i','t','r','a','t','e','\0'
          };

          //! VBR quality. Middle priority
          const Char VBR[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','m','p','3','.','v','b','r','\0'
          };

          //! ABR bitrate. Lowest priority
          const Char ABR[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','m','p','3','.','a','b','r','\0'
          };
          //@}
        }
      }
    }
  }
}
#endif //__SOUND_BACKENDS_PARAMETERS_H_DEFINED__
