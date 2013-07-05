/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//#define DIRECT_ENABLE_DEBUG

#include <list>
#include <map>
#include <string>
#include <vector>

#include <config.h>

extern "C" {
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#ifdef USE_LIBDIRECT
#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>
#endif
}

#ifdef USE_LIBDIRECT
D_DEBUG_DOMAIN( fluxcomp, "fluxcomp", "Flux Compression Tool" );
#define FLUX_D_DEBUG_AT(x...) D_DEBUG_AT(x);
#else

/* fake macros */
#define FLUX_D_DEBUG_AT(x...)
#define D_ASSERT(x...)
#define D_PERROR(x...)
#define D_WARN(x...)          \
     do {                     \
          printf( x );        \
          printf( "\n" );     \
          sleep(5);           \
     } while (0)
#define D_UNIMPLEMENTED(x...)

#define DR_OK     0
#define DR_FAILURE 1
/* fake types */
typedef int DirectResult;

/* fake functions */

DirectResult errno2result( int erno )
{
     if (!errno)
          return DR_OK;

     return DR_FAILURE;
}

void direct_initialize() {};
void direct_shutdown() {};
void direct_print_memleaks() {};

#define direct_log_printf(x...)

#endif

/**********************************************************************************************************************/

static const char *license =
"/*\n"
"   (c) Copyright 2012-2013  DirectFB integrated media GmbH\n"
"   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)\n"
"   (c) Copyright 2000-2004  Convergence (integrated media) GmbH\n"
"\n"
"   All rights reserved.\n"
"\n"
"   Written by Denis Oliver Kropp <dok@directfb.org>,\n"
"              Andreas Shimokawa <andi@directfb.org>,\n"
"              Marek Pikarski <mass@directfb.org>,\n"
"              Sven Neumann <neo@directfb.org>,\n"
"              Ville Syrjälä <syrjala@sci.fi> and\n"
"              Claudio Ciccani <klan@users.sf.net>.\n"
"\n"
"   This library is free software; you can redistribute it and/or\n"
"   modify it under the terms of the GNU Lesser General Public\n"
"   License as published by the Free Software Foundation; either\n"
"   version 2 of the License, or (at your option) any later version.\n"
"\n"
"   This library is distributed in the hope that it will be useful,\n"
"   but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
"   Lesser General Public License for more details.\n"
"\n"
"   You should have received a copy of the GNU Lesser General Public\n"
"   License along with this library; if not, write to the\n"
"   Free Software Foundation, Inc., 59 Temple Place - Suite 330,\n"
"   Boston, MA 02111-1307, USA.\n"
"*/\n";

static const char *filename;


/**********************************************************************************************************************/

class FluxConfig
{
public:
     bool           c_mode;
     bool           identity;
     std::string    include_prefix;
     bool           no_direct;
     bool           call_mode;
     bool           object_ptrs;
     std::string    static_args_bytes;
     bool           dispatch_error_abort;

public:
     FluxConfig()
          :
          c_mode( false ),
          identity( false ),
          no_direct( false ),
          call_mode( false ),
          object_ptrs( false ),
          static_args_bytes( "1000" ),
          dispatch_error_abort( false )
     {
     }

     bool
     parse_command_line( int argc, char *argv[] )
     {
          int n;

          for (n = 1; n < argc; n++) {
               const char *arg = argv[n];

               if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
                    print_usage( argv[0] );
                    return false;
               }

               if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
                    fprintf( stderr, "fluxcomp version %s\n", FLUXCOMP_VERSION );
                    return false;
               }

               if (strcmp (arg, "-V") == 0 || strcmp (arg, "--version-code") == 0) {
                    fprintf( stdout, "%d", FLUXCOMP_MAJOR_VERSION*1000000 + FLUXCOMP_MINOR_VERSION*1000 + FLUXCOMP_MICRO_VERSION );
                    return false;
               }

               if (strcmp (arg, "-c") == 0 || strcmp (arg, "--generate-c") == 0) {
                    c_mode = true;
                    continue;
               }

               if (strcmp (arg, "-i") == 0 || strcmp (arg, "--identity") == 0) {
                    identity = true;
                    continue;
               }

               if (strcmp (arg, "--object-ptrs") == 0) {
                    object_ptrs = true;
                    continue;
               }

               if (strcmp (arg, "--no-direct") == 0) {
                    no_direct = true;
                    continue;
               }
               if (strcmp (arg, "--call-mode") == 0) {
                    call_mode = true;
                    continue;
               }
               if (strncmp (arg, "-p=",3) == 0) {
                    include_prefix = std::string(&arg[3]);
                    continue;
               }
               if (strncmp (arg, "--include-prefix=", 17) == 0) {
                    include_prefix = std::string(&arg[17]);
                    continue;
               }
               if (strncmp (arg, "--static-args-bytes=", 20) == 0) {
                    static_args_bytes = &arg[20];
                    continue;
               }

               if (strcmp (arg, "--dispatch-error-abort") == 0) {
                    dispatch_error_abort = true;
                    continue;
               }

               if (filename || access( arg, R_OK )) {
                    print_usage( argv[0] );
                    return false;
               }

               filename = arg;
          }

          if (!filename) {
               print_usage (argv[0]);
               return false;
          }

          return true;
     }

     void
     print_usage( const char *prg_name )
     {
          fprintf( stderr, "\nFlux Compiler Tool (version %s)\n\n", FLUXCOMP_VERSION );
          fprintf( stderr, "Usage: %s [options] <filename>\n\n", prg_name );
          fprintf( stderr, "Options:\n" );
          fprintf( stderr, "   -h, --help                     Show this help message\n" );
          fprintf( stderr, "   -v, --version                  Print version information\n" );
          fprintf( stderr, "   -V, --version-code             Output version code to stdout\n" );
          fprintf( stderr, "   -c, --generate-c               Generate C instead of C++ code\n" );
          fprintf( stderr, "   -i, --identity                 Generate caller identity tracking code\n" );
          fprintf( stderr, "   --object-ptrs                  Return object pointers rather than IDs\n" );
          fprintf( stderr, "   --call-mode                    Use call mode function to determine call mode\n" );
          fprintf( stderr, "   -p=, --include-prefix=         Override standard include prefix for includes\n" );
          fprintf( stderr, "   --static-args-bytes=           Override standard limit (1000) for stack based arguments\n" );
          fprintf( stderr, "   --dispatch-error-abort         Abort execution when object lookups etc fail\n" );
          fprintf( stderr, "\n" );
     }
};

/**********************************************************************************************************************/

class Entity
{
public:
     Entity( Entity *parent )
          :
          parent( parent )
     {
     }

     typedef std::list<Entity*>   list;
     typedef std::vector<Entity*> vector;

     typedef enum {
          ENTITY_NONE,

          ENTITY_INTERFACE,
          ENTITY_METHOD,
          ENTITY_ARG
     } Type;


     virtual Type GetType() const = 0;


     virtual void Dump() const;

     virtual void SetProperty( const std::string &name, const std::string &value );

private:
     const char    *buf;
     size_t         length;

public:
     Entity::vector  entities;
     Entity         *parent;

     void Open( const char *buf ) {
          this->buf = buf;
     }

     void Close( size_t length ) {
          this->length = length;

          GetEntities( buf, length, entities, this );
     }

     static void GetEntities( const char     *buf,
                              size_t          length,
                              Entity::vector &out_vector,
                              Entity         *parent );

     static DirectResult GetEntities( const char     *filename,
                                      Entity::vector &out_vector );
};

class Interface : public Entity
{
public:
     Interface( Entity *parent )
          :
          Entity( parent ),
          buffered( false ),
          core( "CoreDFB" )
     {
     }


     virtual Type GetType() const { return ENTITY_INTERFACE; }


     virtual void Dump() const;

     virtual void SetProperty( const std::string &name, const std::string &value );


     std::string              name;
     std::string              version;
     std::string              object;
     std::string              dispatch;
     bool                     buffered;
     std::string              core;
};

class Method : public Entity
{
public:
     Method( Entity *parent )
          :
          Entity( parent ),
          async( false ),
          queue( false ),
          buffer( false )
     {
     }


     virtual Type GetType() const { return ENTITY_METHOD; }


     virtual void Dump() const;

     virtual void SetProperty( const std::string &name, const std::string &value );


     std::string              name;
     bool                     async;
     bool                     queue;
     bool                     buffer;

public:
     static std::string
     PrintParam( std::string string_buffer,
                 std::string type,
                 std::string ptr,
                 std::string name,
                 bool        first )
     {
          char buf[1000];

          snprintf( buf, sizeof(buf), "%s                    %-40s %2s%s", first ? "" : ",\n", type.c_str(), ptr.c_str(), name.c_str() );

          return string_buffer + buf;
     }

     static std::string
     PrintMember( std::string string_buffer,
                          std::string type,
                          std::string ptr,
                          std::string name )
     {
          char buf[1000];

          snprintf( buf, sizeof(buf), "    %-40s %2s%s;\n", type.c_str(), ptr.c_str(), name.c_str() );

          return string_buffer + buf;
     }

     std::string ArgumentsAsParamDecl() const;
     std::string ArgumentsAsMemberDecl() const;
     std::string ArgumentsOutputAsMemberDecl( const FluxConfig &config ) const;
     std::string ArgumentsAsMemberParams() const;

     std::string ArgumentsInputAssignments() const;
     std::string ArgumentsOutputAssignments() const;

     std::string ArgumentsAssertions() const;

     std::string ArgumentsOutputObjectDecl() const;
     std::string ArgumentsOutputTmpDecl() const;
     std::string ArgumentsInputObjectDecl() const;

     std::string ArgumentsOutputObjectCatch( const FluxConfig &config ) const;
     std::string ArgumentsOutputObjectThrow( const FluxConfig &config ) const;
     std::string ArgumentsInoutReturn() const;
     std::string ArgumentsOutputTmpReturn() const;

     std::string ArgumentsInputObjectLookup( const FluxConfig &config ) const;
     std::string ArgumentsInputObjectUnref() const;

     std::string ArgumentsInputDebug( const FluxConfig &config,
                                      const Interface  *face ) const;

     std::string ArgumentsNames() const;
     std::string ArgumentsSize( const Interface *face, bool output ) const;
     std::string ArgumentsSizeReturn( const Interface *face ) const;

     std::string OpenSplitArgumentsIfRequired() const;
     std::string CloseSplitArgumentsIfRequired() const;
};

class Arg : public Entity
{
public:
     Arg( Entity *parent )
          :
          Entity( parent ),
          optional( false ),
          array( false ),
          split( false )
     {
     }


     virtual Type GetType() const { return ENTITY_ARG; }


     virtual void Dump() const;

     virtual void SetProperty( const std::string &name, const std::string &value );


     std::string              name;
     std::string              direction;
     std::string              type;
     std::string              type_name;
     bool                     optional;

     bool                     array;
     std::string              count;
     std::string              max;

     bool                     split;

public:
     std::string param_name() const
     {
          if (direction == "output")
               return std::string("ret_") + name;

          return name;
     }

     std::string formatCharacter() const
     {
          if (type == "int") {
               if (type_name == "u8"  ||
                   type_name == "u16" ||
                   type_name == "u32")
                    return "u";

               if (type_name == "s8"  ||
                   type_name == "s16" ||
                   type_name == "s32")
                    return "d";

               if (type_name == "u64")
                    return "llu";

               if (type_name == "s64")
                    return "lld";

               if (type_name == "void*")
                    return "p";

               D_WARN( "unrecognized integer type '%s'", type_name.c_str() );

               return "lu";
          }

          if (type == "enum")
               return "x";

          D_WARN( "unsupported type '%s'", type.c_str() );

          D_UNIMPLEMENTED();

          return "x";
     }

     std::string size( bool use_args ) const
     {
          if (array) {
               if (use_args)
                    return std::string("args->") + count + " * sizeof(" + type_name + ")";
               else
                    return (split ? std::string( "num_records" ) : count) + " * sizeof(" + type_name + ")";
          }

          return std::string("sizeof(") + type_name + ")";
     }

     std::string sizeReturn( const Method *method ) const
     {
          if (array) {
               /* Lookup 'count' argument */
               for (Entity::vector::const_iterator iter = method->entities.begin(); iter != method->entities.end(); iter++) {
                    const Arg *arg = dynamic_cast<const Arg*>( *iter );

                    if (arg->name == count) {
                         if (arg->direction == "input")
                              return std::string("args->") + count + " * sizeof(" + type_name + ")";

                         return std::string("return_args->") + count + " * sizeof(" + type_name + ")";
                    }
               }
          }

          return std::string("sizeof(") + type_name + ")";
     }

     std::string sizeMax( bool use_args ) const
     {
          if (array) {
               if (use_args)
                    return std::string("args->") + max + " * sizeof(" + type_name + ")";
               else
                    return max + " * sizeof(" + type_name + ")";
          }

          return std::string("sizeof(") + type_name + ")";
     }

     std::string offset( const Method *method, bool use_args, bool output ) const
     {
          D_ASSERT( array == true );

          std::string result;

          for (Entity::vector::const_iterator iter = method->entities.begin(); iter != method->entities.end(); iter++) {
               const Arg *arg = dynamic_cast<const Arg*>( *iter );

               if (!arg->array)
                    continue;

               FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

               if (arg == this)
                    break;

               if (output) {
                    if (arg->direction == "output" || arg->direction == "inout")
                         result += std::string(" + return_args->") + arg->size( false );
               }
               else {
                    if (arg->direction == "input")
                         result += std::string(" + ") + arg->size( use_args );
               }
          }

          return result;
     }
};

/**********************************************************************************************************************/

void
Entity::Dump() const
{
     direct_log_printf( NULL, "\n" );
     direct_log_printf( NULL, "Entity (TYPE %d)\n", GetType() );
     direct_log_printf( NULL, "  Buffer at        %p [%zu]\n", buf, length );
}

void
Interface::Dump() const
{
     Entity::Dump();

     direct_log_printf( NULL, "  Name             %s\n", name.c_str() );
     direct_log_printf( NULL, "  Version          %s\n", version.c_str() );
     direct_log_printf( NULL, "  Object           %s\n", object.c_str() );
     direct_log_printf( NULL, "  Dispatch         %s\n", dispatch.c_str() );
}

void
Method::Dump() const
{
     Entity::Dump();

     direct_log_printf( NULL, "  Name             %s\n", name.c_str() );
}

void
Arg::Dump() const
{
     Entity::Dump();

     direct_log_printf( NULL, "  Name             %s\n", name.c_str() );
     direct_log_printf( NULL, "  Direction        %s\n", direction.c_str() );
     direct_log_printf( NULL, "  Type             %s\n", type.c_str() );
     direct_log_printf( NULL, "  Typename         %s\n", type_name.c_str() );
}

/**********************************************************************************************************************/

void
Entity::SetProperty( const std::string &name,
                     const std::string &value )
{
}

void
Interface::SetProperty( const std::string &name,
                        const std::string &value )
{
     if (name == "name") {
          this->name = value;
          return;
     }

     if (name == "version") {
          version = value;
          return;
     }

     if (name == "object") {
          object = value;
          dispatch = value;
          return;
     }

     if (name == "dispatch") {
          dispatch = value;
          return;
     }

     if (name == "core") {
          core = value;
          return;
     }
}

void
Method::SetProperty( const std::string &name,
                     const std::string &value )
{
     if (name == "name") {
          this->name = value;
          return;
     }

     if (name == "async") {
          async = value == "yes";
          return;
     }

     if (name == "queue") {
          queue = value == "yes";
          return;
     }

     if (name == "buffer") {
          buffer = value == "yes";
          return;
     }
}

void
Arg::SetProperty( const std::string &name,
                  const std::string &value )
{
     if (name == "name") {
          this->name = value;
          return;
     }

     if (name == "direction") {
          direction = value;
          return;
     }

     if (name == "type") {
          type = value;
          return;
     }

     if (name == "typename") {
          type_name = value;
          return;
     }

     if (name == "optional") {
          optional = value == "yes";
          return;
     }

     if (name == "count") {
          array = true;
          count = value;
          return;
     }

     if (name == "max") {
          max = value;
          return;
     }

     if (name == "split" && value == "yes") {
          split = true;
          return;
     }
}

/**********************************************************************************************************************/

void
Entity::GetEntities( const char     *buf,
                     size_t          length,
                     Entity::vector &out_vector,
                     Entity         *parent )
{
     size_t       i;
     unsigned int level   = 0;
     bool         quote   = false;
     bool         comment = false;

     std::string                        name;
     std::map<unsigned int,std::string> names;

     Entity *entity = NULL;

     FLUX_D_DEBUG_AT( fluxcomp, "%s( buf %p, length %zu )\n", __func__, buf, length );

     for (i=0; i<length; i++) {
          FLUX_D_DEBUG_AT( fluxcomp, "%*s[%u]  -> '%c' <-\n", level*2, "", level, buf[i] );

          if (comment) {
               switch (buf[i]) {
                    case '\n':
                         comment = false;
                         break;

                    default:
                         break;
               }
          }
          else if (quote) {
               switch (buf[i]) {
                    // TODO: implement escaped quotes in strings
                    case '"':
                         quote = false;
                         break;

                    default:
                         name += buf[i];
               }
          }
          else {
               switch (buf[i]) {
                    case '"':
                         quote = true;
                         break;

                    case '#':
                         comment = true;
                         break;

                    case '.':
                    case '-':
                    case '_':
                    case 'a' ... 'z':
                    case 'A' ... 'Z':
                    case '0' ... '9':
                         name += buf[i];
                         break;

                    default:
                         if (!name.empty()) {
                              FLUX_D_DEBUG_AT( fluxcomp, "%*s=-> name = '%s'\n", level*2, "", name.c_str() );

                              if (!names[level].empty()) {
                                   switch (level) {
                                        case 1:
                                             FLUX_D_DEBUG_AT( fluxcomp, "%*s#### setting property '%s' = '%s'\n",
                                                         level*2, "", names[level].c_str(), name.c_str() );

                                             D_ASSERT( entity != NULL );

                                             entity->SetProperty( names[level], name );
                                             break;

                                        default:
                                             break;
                                   }

                                   name = "";
                              }

                              names[level] = name;
                              name         = "";
                         }

                         switch (buf[i]) {
                              case '{':
                              case '}':
                                   switch (buf[i]) {
                                        case '{':
                                             switch (level) {
                                                  case 0:
                                                       if (names[level] == "interface") {
                                                            D_ASSERT( entity == NULL );

                                                            entity = new Interface( parent );

                                                            entity->Open( &buf[i + 1] );

                                                            FLUX_D_DEBUG_AT( fluxcomp, "%*s#### open entity %p (Interface)\n", level*2, "", entity );
                                                       }
                                                       if (names[level] == "method") {
                                                            D_ASSERT( entity == NULL );

                                                            entity = new Method( parent );

                                                            entity->Open( &buf[i + 1] );

                                                            FLUX_D_DEBUG_AT( fluxcomp, "%*s#### open entity %p (Method)\n", level*2, "", entity );
                                                       }
                                                       if (names[level] == "arg") {
                                                            D_ASSERT( entity == NULL );

                                                            entity = new Arg( parent );

                                                            entity->Open( &buf[i + 1] );

                                                            FLUX_D_DEBUG_AT( fluxcomp, "%*s#### open entity %p (Arg)\n", level*2, "", entity );
                                                       }
                                                       break;

                                                  default:
                                                       break;
                                             }

                                             names[level] = "";

                                             level++;
                                             break;

                                        case '}':
                                             D_ASSERT( names[level].empty() );

                                             level--;

                                             switch (level) {
                                                  case 0:
                                                       FLUX_D_DEBUG_AT( fluxcomp, "%*s#### close entity %p\n", level*2, "", entity );

                                                       D_ASSERT( entity != NULL );

                                                       entity->Close( &buf[i-1] - entity->buf );

                                                       out_vector.push_back( entity );

                                                       entity = NULL;
                                                       break;

                                                  case 1:
                                                       break;

                                                  default:
                                                       break;
                                             }
                                             break;
                                   }

                                   FLUX_D_DEBUG_AT( fluxcomp, "%*s=-> level => %u\n", level*2, "", level );
                                   break;

                              case ' ':
                              case '\t':
                              case '\n':
                              case '\r':
                                   break;

                              default:
                                   break;
                         }
                         break;
               }
          }
     }
}

DirectResult
Entity::GetEntities( const char     *filename,
                     Entity::vector &out_vector )
{
     int          ret = DR_OK;
     int          fd;
     struct stat  stat;
     void        *ptr = MAP_FAILED;

     /* Open the file. */
     fd = open( filename, O_RDONLY );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "GetEntities: Failure during open() of '%s'!\n", filename );
          return (DirectResult) ret;
     }

     /* Query file size etc. */
     if (fstat( fd, &stat ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "GetEntities: Failure during fstat() of '%s'!\n", filename );
          goto out;
     }

     /* Memory map the file. */
     ptr = mmap( NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0 );
     if (ptr == MAP_FAILED) {
          ret = errno2result( errno );
          D_PERROR( "GetEntities: Failure during mmap() of '%s'!\n", filename );
          goto out;
     }


     Entity::GetEntities( (const char*) ptr, stat.st_size, out_vector, NULL );


out:
     if (ptr != MAP_FAILED)
          munmap( ptr, stat.st_size );

     close( fd );

     return (DirectResult) ret;
}

/**********************************************************************************************************************/

std::string
Method::ArgumentsAsParamDecl() const
{
     std::string result;
     bool        first = true;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "struct") {
               if (arg->direction == "input")
                    result = PrintParam( result, std::string("const ") + arg->type_name, "*", arg->param_name(), first );
               else
                    result = PrintParam( result, arg->type_name, "*", arg->param_name(), first );
          }
          else if (arg->type == "enum" || arg->type == "int") {
               if (arg->direction == "input") {
                    if (arg->array)
                         result = PrintParam( result, std::string("const ") + arg->type_name, "*", arg->param_name(), first );
                    else
                         result = PrintParam( result, arg->type_name, "", arg->param_name(), first );
               }
               else
                    result = PrintParam( result, arg->type_name, "*", arg->param_name(), first );
          }
          else if (arg->type == "object") {
               if (arg->direction == "input")
                    result = PrintParam( result, arg->type_name, "*", arg->param_name(), first );
               else
                    result = PrintParam( result, arg->type_name, "**", arg->param_name(), first );
          }

          if (first)
               first = false;
     }

     return result;
}

std::string
Method::ArgumentsAsMemberDecl() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (arg->array)
               continue;

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "input" || arg->direction == "inout") {
               if (arg->optional)
                    result = PrintMember( result, "bool", "", arg->name + "_set" );

               if (arg->type == "struct")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "enum")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "int")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "object")
                    result = PrintMember( result, "u32", "", arg->name + "_id" );
          }
     }

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          char       buf[300];
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (!arg->array)
               continue;

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "input" || arg->direction == "inout") {
               if (arg->optional)
                    result = PrintMember( result, "bool", "", arg->name + "_set" );

               snprintf( buf, sizeof(buf), "    /* '%s' %s follow (%s) */\n", arg->count.c_str(), arg->type_name.c_str(), arg->name.c_str() );

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsOutputAsMemberDecl( const FluxConfig &config ) const
{
     std::string result;

     result = PrintMember( result, "DFBResult", "", "result" );

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (arg->array)
               continue;

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "output" || arg->direction == "inout") {
               if (arg->type == "struct")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "enum")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "int")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "object") {
                    result = PrintMember( result, "u32", "", arg->name + "_id" );

                    if (config.object_ptrs)
                         result = PrintMember( result, "void*", "", arg->name + "_ptr" );
               }
          }
     }

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          char       buf[300];
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (!arg->array)
               continue;

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "output" || arg->direction == "inout") {
               if (arg->optional)
                    result = PrintMember( result, "bool", "", arg->name + "_set" );

               snprintf( buf, sizeof(buf), "    /* '%s' %s follow (%s) */\n", arg->count.c_str(), arg->type_name.c_str(), arg->name.c_str() );

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsAsMemberParams() const
{
     std::string result;
     bool        first = true;
     bool        second_output_array = false;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (first)
               first = false;
          else
               result += ", ";

          if (arg->direction == "input" || arg->direction == "inout") {
               if (arg->optional)
                    result += std::string("args->") + arg->name + "_set ? ";

               if (arg->array) {
                    if (arg->type == "struct" || arg->type == "enum" || arg->type == "int")
                         result += std::string("(") + arg->type_name + "*) ((char*)(args + 1)" + arg->offset( this, true, false ) + ")";
                    else if (arg->type == "object")
                         D_UNIMPLEMENTED();
               }
               else {
                    if (arg->type == "struct")
                         result += std::string("&args->") + arg->name;
                    else if (arg->type == "enum")
                         result += std::string("args->") + arg->name;
                    else if (arg->type == "int")
                         result += std::string("args->") + arg->name;
                    else if (arg->type == "object")
                         result += arg->name;
               }

               if (arg->optional)
                    result += std::string(" : NULL");
          }

          if (arg->direction == "output") {
               if (arg->optional)
                    D_UNIMPLEMENTED();

               if (arg->array) {
                    if (second_output_array) {
                         result += "tmp_" + arg->name;
                    }
                    else {
                         if (arg->type == "struct" || arg->type == "enum" || arg->type == "int")
                              result += std::string("(") + arg->type_name + "*) ((char*)(return_args + 1)" + arg->offset( this, true, true ) + ")";
                         else if (arg->type == "object")
                              D_UNIMPLEMENTED();

                         second_output_array = true;
                    }
               }
               else {
                    if (arg->type == "struct")
                         result += std::string("&return_args->") + arg->name;
                    else if (arg->type == "enum")
                         result += std::string("&return_args->") + arg->name;
                    else if (arg->type == "int")
                         result += std::string("&return_args->") + arg->name;
                    else if (arg->type == "object")
                         result += std::string("&") + arg->name;
               }
          }
     }

     return result;
}

std::string
Method::ArgumentsInputAssignments() const
{
     std::string result;
     bool        split = false;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (arg->split) {
               split = true;
               break;
          }
     }

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (arg->array)
               continue;

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "input" || arg->direction == "inout") {
               if (arg->optional)
                    result += std::string("  if (") + arg->name + ") {\n";

               if (arg->type == "struct")
                    result += std::string("    args->") + arg->name + " = *" + arg->name + ";\n";
               else if (arg->type == "enum")
                    result += std::string("    args->") + arg->name + " = " + arg->name + ";\n";
               else if (arg->type == "int")
                    result += std::string("    args->") + arg->name + " = " + (split ? (arg->name == "num" ? std::string( "num_records" ) : arg->name) : arg->name) + ";\n";
               else if (arg->type == "object")
                    result += std::string("    args->") + arg->name + "_id = " + arg->type_name + "_GetID( " + arg->name + " );\n";

               if (arg->optional) {
                    result += std::string("    args->") + arg->name + "_set = true;\n";
                    result += std::string("  }\n");
                    result += std::string("  else\n");
                    result += std::string("    args->") + arg->name + "_set = false;\n";
               }
          }
     }

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (!arg->array)
               continue;

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "input" || arg->direction == "inout") {
               if (arg->optional)
                    result += std::string("  if (") + arg->name + ") {\n";

               if (arg->type == "struct" || arg->type == "enum" || arg->type == "int")
                    result += std::string("    direct_memcpy( (char*) (args + 1)") + arg->offset( this, false, false ) + ", " + arg->name + ", " + arg->size( false ) + " );\n";
               else if (arg->type == "object")
                    D_UNIMPLEMENTED();

               if (arg->optional) {
                    result += std::string("    args->") + arg->name + "_set = true;\n";
                    result += std::string("  }\n");
                    result += std::string("  else {\n");
                    result += std::string("    args->") + arg->name + "_set = false;\n";
                    result += std::string("    ") + arg->name + " = 0;\n"; // FIXME: this sets num to 0 to avoid dispatch errors, but what if num is before this?
                    result += std::string("  }\n");
               }
          }
     }

     return result;
}

std::string
Method::ArgumentsOutputAssignments() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (arg->array)
               continue;

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "output" || arg->direction == "inout") {
               if (arg->type == "struct" || arg->type == "enum" || arg->type == "int") {
                    if (arg->optional)
                         result += std::string("    if (") + arg->param_name() + ")\n";

                    result += std::string( arg->optional ? "        *" : "    *") + arg->param_name() + " = return_args->" + arg->name + ";\n";
               }
          }
     }

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (!arg->array)
               continue;

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "output" || arg->direction == "inout") {
               if (arg->optional)
                    D_UNIMPLEMENTED();

               if (arg->type == "struct" || arg->type == "enum" || arg->type == "int")
                    result += std::string("    direct_memcpy( ret_") + arg->name + ", (char*) (return_args + 1)" + arg->offset( this, false, true ) + ", " + arg->sizeReturn( this ) + " );\n";
               else if (arg->type == "object")
                    D_UNIMPLEMENTED();
          }
     }

     return result;
}

std::string
Method::OpenSplitArgumentsIfRequired() const
{
     std::string result;
     std::string record_sizes;
     int         split = 0;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (arg->split) {
               if (split)
                    record_sizes += std::string( " + " );

               record_sizes += std::string( "sizeof(" ) + arg->type_name + std::string( ")" );
               split++;
          }
     }

     if (!split)
          return result;

     result += std::string( "const u32 record_size = " ) + record_sizes + std::string( ";\n" );
     result += std::string( "const u32 max_records = " ) + std::string( "CALLBUFFER_FUSION_MESSAGE_SIZE / record_size;\n" );
     result += std::string( "for (u32 i = 0; i < num; i+= max_records) {\n" );
     result += std::string( "    const u32 num_records = num > max_records ? max_records : num;\n" );

     return result;
}

std::string
Method::CloseSplitArgumentsIfRequired() const
{
     std::string result;
     bool        split = false;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (arg->split) {
               split = true;
               break;
          }
     }

     if (split)
          result += "}";

     return result;
}

std::string
Method::ArgumentsAssertions() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if ((arg->type == "struct" || arg->type == "object") && !arg->optional)
               result += std::string("    D_ASSERT( ") + arg->param_name() + " != NULL );\n";
     }

     return result;
}

std::string
Method::ArgumentsOutputObjectDecl() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "output")
               result += std::string("    ") + arg->type_name + " *" + arg->name + " = NULL;\n";
     }

     return result;
}

std::string
Method::ArgumentsOutputTmpDecl() const
{
     std::string result;
     bool        second = false;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "output" && arg->array) {
               if (second)
                    result += std::string("    ") + arg->type_name + "  tmp_" + arg->name + "[" + arg->max + "];\n";
               else
                    second = true;
          }
     }

     return result;
}

std::string
Method::ArgumentsInputObjectDecl() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "input")
               result += std::string("    ") + arg->type_name + " *" + arg->name + " = NULL;\n";
     }

     return result;
}

std::string
Method::ArgumentsOutputObjectCatch( const FluxConfig &config ) const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "output") {
               char buf[1000];

               snprintf( buf, sizeof(buf),
                         "    ret = (DFBResult) %s_Catch( %s, return_args->%s%s, &%s );\n"
                         "    if (ret) {\n"
                         "         D_DERROR( ret, \"%%s: Catching %s by ID %%u failed!\\n\", __FUNCTION__, return_args->%s_id );\n"
                         "         goto out;\n"
                         "    }\n"
                         "\n"
                         "    *%s = %s;\n"
                         "\n",
                         arg->type_name.c_str(), config.c_mode ? "core_dfb" : "core", arg->name.c_str(), config.object_ptrs ? "_ptr" : "_id", arg->name.c_str(),
                         arg->name.c_str(), arg->name.c_str(),
                         arg->param_name().c_str(), arg->name.c_str() );

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsOutputObjectThrow( const FluxConfig &config ) const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "output") {
               char buf[1000];

               snprintf( buf, sizeof(buf),
                         "                %s_Throw( %s, caller, &return_args->%s_id );\n",
                         arg->type_name.c_str(), arg->name.c_str(), arg->name.c_str() );

               result += buf;

               if (config.object_ptrs) {
                    snprintf( buf, sizeof(buf),
                              "                return_args->%s_ptr = (void*) %s;\n",
                              arg->name.c_str(), arg->name.c_str() );

                    result += buf;
               }
          }
     }

     return result;
}

std::string
Method::ArgumentsInoutReturn() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "inout") {
               char buf[1000];

               snprintf( buf, sizeof(buf),
                         "                return_args->%s = args->%s;\n",
                         arg->name.c_str(), arg->name.c_str() );

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsOutputTmpReturn() const
{
     std::string result;
     bool        second_output_array = false;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "output" && arg->array) {
               if (second_output_array) {
                    char        buf[1000];
                    std::string dst = std::string("(char*) ((char*)(return_args + 1)") + arg->offset( this, true, true ) + ")";

                    snprintf( buf, sizeof(buf),
                              "                direct_memcpy( %s, tmp_%s, return_args->%s_size );\n",
                              dst.c_str(), arg->name.c_str(), arg->name.c_str() );

                    result += buf;
               }
               else
                    second_output_array = true;
          }
     }

     return result;
}

std::string
Method::ArgumentsInputObjectLookup( const FluxConfig &config ) const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "input") {
               char buf[1000];

               if (arg->optional) {
                    snprintf( buf, sizeof(buf),
                              "            if (args->%s_set) {\n"
                              "                ret = (DFBResult) %s_Lookup( core_dfb, args->%s_id, caller, &%s );\n"
                              "                if (ret) {\n"
                              "                     D_DERROR( ret, \"%%s(%s): Looking up %s by ID %%u failed!\\n\", __FUNCTION__, args->%s_id );\n"
                              "%s"
                              "%s"
                              "                     return ret;\n"
                              "                }\n"
                              "            }\n"
                              "\n",
                              arg->name.c_str(),
                              arg->type_name.c_str(), arg->name.c_str(), arg->name.c_str(),
                              name.c_str(), arg->name.c_str(), arg->name.c_str(),
                              async ? "" : "                     return_args->result = ret;\n",
                              config.dispatch_error_abort ? "                     D_BREAK( \"could not lookup object\" );\n" : "" );
               }
               else {
                    snprintf( buf, sizeof(buf),
                              "            ret = (DFBResult) %s_Lookup( core_dfb, args->%s_id, caller, &%s );\n"
                              "            if (ret) {\n"
                              "                 D_DERROR( ret, \"%%s(%s): Looking up %s by ID %%u failed!\\n\", __FUNCTION__, args->%s_id );\n"
                              "%s"
                              "%s"
                              "                 return ret;\n"
                              "            }\n"
                              "\n",
                              arg->type_name.c_str(), arg->name.c_str(), arg->name.c_str(),
                              name.c_str(), arg->name.c_str(), arg->name.c_str(),
                              async ? "" : "                 return_args->result = ret;\n",
                              config.dispatch_error_abort ? "                 D_BREAK( \"could not lookup object\" );\n" : "" );
               }

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsInputDebug( const FluxConfig &config,
                             const Interface  *face ) const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (arg->array)
               continue;

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "input" || arg->direction == "inout") {
               if (arg->optional)
                    result += std::string("  if (args->") + arg->name + "_set)\n";

               if (arg->type == "struct")
                    result += std::string("         ;    // TODO: ") + arg->type_name + "_debug args->" + arg->name + ";\n";
               else if (arg->type == "object") {
                    char buf[1000];

                    snprintf( buf, sizeof(buf),
                              "            D_DEBUG_AT( DirectFB_%s, \"  -> %s = %%d\\n\", args->%s_id );\n",
                              face->object.c_str(), arg->name.c_str(), arg->name.c_str() );

                    result += buf;
               }
               else if (arg->type == "enum" || arg->type == "int") {
                    char buf[1000];

                    snprintf( buf, sizeof(buf),
                              "            D_DEBUG_AT( DirectFB_%s, \"  -> %s = %%%s\\n\", args->%s );\n",
                              face->object.c_str(), arg->name.c_str(), arg->formatCharacter().c_str(), arg->name.c_str() );

                    result += buf;
               }
               else
                    result += std::string("         ;    // TODO: ") + arg->type + " debug\n";
          }
     }

     return result;
}

std::string
Method::ArgumentsInputObjectUnref() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "input") {
               char buf[1000];

               snprintf( buf, sizeof(buf),
                         "            if (%s)\n"
                         "                %s_Unref( %s );\n"
                         "\n",
                         arg->name.c_str(), arg->type_name.c_str(), arg->name.c_str() );

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsNames() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg  = dynamic_cast<const Arg*>( *iter );
          bool       last = arg == entities[entities.size()-1];

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          result += arg->param_name();

          if (!last)
               result += ", ";
     }

     return result;
}

std::string
Method::ArgumentsSize( const Interface *face, bool output ) const
{
     std::string result = "sizeof(";
     bool        split  = false;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (arg->split) {
               split = true;
               break;
          }
     }

     if (output)
          result += face->object + name + "Return)";
     else
          result += face->object + name + ")";

     if (split)
          return result += std::string( " + record_size * num_records" );

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (!arg->array)
               continue;

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (output) {
               if (arg->direction == "output" || arg->direction == "inout")
                    result += " + " + arg->sizeMax( false );
          }
          else {
               if (arg->direction == "input" || arg->direction == "inout")
                    result += " + " + arg->size( false );
          }
     }

     return result;
}

std::string
Method::ArgumentsSizeReturn( const Interface *face ) const
{
     std::string result = "sizeof(";

     result += face->object + name + "Return)";

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (!arg->array)
               continue;

          FLUX_D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "output" || arg->direction == "inout")
               result += " + " + arg->sizeReturn( this );
     }

     return result;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

class FluxComp
{
public:
     void GenerateHeader( const Interface   *face,
                          const FluxConfig  &config );

     void GenerateSource( const Interface   *face,
                          const FluxConfig  &config );

     void PrintInterface( FILE              *file,
                          const Interface   *face,
                          const std::string &name,
                          const std::string &super,
                          bool               abstract );
};

/**********************************************************************************************************************/

void
FluxComp::GenerateHeader( const Interface *face, const FluxConfig &config )
{
     FILE        *file;
     std::string  filename = face->object + ".h";

     file = fopen( filename.c_str(), "w" );
     if (!file) {
          D_PERROR( "FluxComp: fopen( '%s' ) failed!\n", filename.c_str() );
          return;
     }

     fprintf( file, "/*\n"
                    " *	This file was automatically generated by fluxcomp; DO NOT EDIT!\n"
                    " */\n"
                    "%s\n"
                    "#ifndef ___%s__H___\n"
                    "#define ___%s__H___\n"
                    "\n"
                    "#include %s%s%s%s_includes.h%s\n"
                    "\n"
                    "/**********************************************************************************************************************\n"
                    " * %s\n"
                    " */\n"
                    "\n"
                    "#ifdef __cplusplus\n"
                    "#include <core/Interface.h>\n"
                    "\n"
                    "extern \"C\" {\n"
                    "#endif\n"
                    "\n"
                    "\n",
              license,
              face->object.c_str(),
              face->object.c_str(),
              config.include_prefix.empty() ? "\"" : "<", config.include_prefix.c_str(),
              config.include_prefix.empty() ? "" : "/", face->object.c_str(),
              config.include_prefix.empty() ? "\"" : ">",
              face->object.c_str() );

     /* C Wrappers */

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          fprintf( file, "DFBResult %s_%s(\n"
                         "                    %-40s  *obj%s\n"
                         "%s"
                         ");\n"
                         "\n",
                   face->object.c_str(), method->name.c_str(),
                   face->object.c_str(), method->entities.empty() ? "" : ",",
                   method->ArgumentsAsParamDecl().c_str() );
     }


     fprintf( file, "\n"
                    "void %s_Init_Dispatch(\n"
                    "                    %-20s *core,\n"
                    "                    %-20s *obj,\n"
                    "                    FusionCall           *call\n"
                    ");\n"
                    "\n",
              face->object.c_str(), face->core.c_str(), face->dispatch.c_str() );

     fprintf( file, "void  %s_Deinit_Dispatch(\n"
                    "                    FusionCall           *call\n"
                    ");\n"
                    "\n",
              face->object.c_str() );


     fprintf( file, "\n"
                    "#ifdef __cplusplus\n"
                    "}\n"
                    "%s"
                    "\n"
                    "\n"
                    "%s"
                    "\n"
                    "\n"
                    "/*\n"
                    " * %s Calls\n"
                    " */\n"
                    "typedef enum {\n",
               config.c_mode ? "#endif\n" : "",
              !config.c_mode ? "namespace DirectFB {\n" : "",
              face->object.c_str() );

     /* Method IDs */

     int index = 1;

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          fprintf( file, "    %s%s_%s = %d,\n",
                   config.c_mode ? "_" : "", face->object.c_str(), method->name.c_str(), index++ );
     }

     fprintf( file, "} %sCall;\n"
                    "\n",
              face->object.c_str() );


     /* Method Argument Structures */

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          fprintf( file, "/*\n"
                         " * %s_%s\n"
                         " */\n"
                         "typedef struct {\n"
                         "%s"
                         "} %s%s;\n"
                         "\n"
                         "typedef struct {\n"
                         "%s"
                         "} %s%sReturn;\n"
                         "\n"
                         "\n",
                   face->object.c_str(), method->name.c_str(),
                   method->ArgumentsAsMemberDecl().c_str(),
                   face->object.c_str(), method->name.c_str(),
                   method->ArgumentsOutputAsMemberDecl( config ).c_str(),
                   face->object.c_str(), method->name.c_str() );
     }


     /* Abstract Interface */

     if (!config.c_mode) {
          PrintInterface( file, face, face->name, "Interface", true );


          /* Real Interface */

          fprintf( file, "\n"
                         "\n"
                         "\n"
                         "class %s_Real : public %s\n"
                         "{\n"
                         "private:\n"
                         "    %s *obj;\n"
                         "\n"
                         "public:\n"
                         "    %s_Real( %s *core, %s *obj )\n"
                         "        :\n"
                         "        %s( core ),\n"
                         "        obj( obj )\n"
                         "    {\n"
                         "    }\n"
                         "\n",
                   face->name.c_str(), face->name.c_str(), face->dispatch.c_str(),
                   face->name.c_str(), face->core.c_str(), face->dispatch.c_str(), face->name.c_str() );
     }

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          if (!config.c_mode) {
               fprintf( file, "    virtual DFBResult %s(\n"
                              "%s\n"
                              "    );\n"
                              "\n",
                        method->name.c_str(),
                        method->ArgumentsAsParamDecl().c_str() );
          }
          else  {
               fprintf( file, "DFBResult %s_Real__%s( %s *obj%s\n"
                              "%s );\n"
                              "\n",
                        face->name.c_str(), method->name.c_str(), face->dispatch.c_str(), method->ArgumentsAsParamDecl().empty() ? "" : ",",
                        method->ArgumentsAsParamDecl().c_str() );
          }
     }

     if (!config.c_mode) {
          fprintf( file, "};\n" );


          /* Requestor Interface */

          fprintf( file, "\n"
                         "\n"
                         "\n"
                         "class %s_Requestor : public %s%s\n"
                         "{\n"
                         "private:\n"
                         "    %s *obj;\n"
                         "\n"
                         "public:\n"
                         "    %s_Requestor( %s *core, %s *obj )\n"
                         "        :\n"
                         "        %s( core ),\n"
                         "%s"
                         "        obj( obj )\n"
                         "    {\n"
                         "    }\n"
                         "\n",
                   face->name.c_str(), face->name.c_str(), face->buffered ? ", public CallBuffer" : "", face->object.c_str(),
                   face->name.c_str(), face->core.c_str(), face->object.c_str(), face->name.c_str(),
                   face->buffered ? "        CallBuffer( 16000 ),\n" : "" );
     }

     if (face->buffered)
          fprintf( file, "protected:\n"
                         "    virtual DFBResult flushCalls();\n"
                         "\n" );

     if (!config.c_mode)
          fprintf( file, "public:\n" );

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          if (!config.c_mode) {
               fprintf( file, "    virtual DFBResult %s(\n"
                              "%s\n"
                              "    );\n"
                              "\n",
                        method->name.c_str(),
                        method->ArgumentsAsParamDecl().c_str() );
          }
          else {

               fprintf( file, "DFBResult %s_Requestor__%s( %s *obj%s\n"
                              "%s );\n"
                              "\n",
                        face->name.c_str(), method->name.c_str(), face->object.c_str(), method->ArgumentsAsParamDecl().empty() ? "" : ",",
                        method->ArgumentsAsParamDecl().c_str() );
          }
     }

     if (!config.c_mode)
          fprintf( file, "};\n"
                         "\n" );

     fprintf( file, "\n"
                    "DFBResult %sDispatch__Dispatch( %s *obj,\n"
                    "                    FusionID      caller,\n"
                    "                    int           method,\n"
                    "                    void         *ptr,\n"
                    "                    unsigned int  length,\n"
                    "                    void         *ret_ptr,\n"
                    "                    unsigned int  ret_size,\n"
                    "                    unsigned int *ret_length );\n",
              face->object.c_str(), face->dispatch.c_str() );


     if (!config.c_mode)
          fprintf( file, "\n"
                         "}\n" );

     fprintf( file, "\n"
                    "\n"
                    "#endif\n" );

     if (!config.c_mode)
          fprintf( file, "\n"
                         "#endif\n" );

     fclose( file );
}

void
FluxComp::GenerateSource( const Interface *face, const FluxConfig &config )
{
     FILE        *file;
     std::string  filename = face->object;
     bool         direct   = true;

     if (!config.c_mode)
          filename += ".cpp";
     else
          filename += ".c";

     if (config.no_direct || face->object != face->dispatch)
          direct = false;

     file = fopen( filename.c_str(), "w" );
     if (!file) {
          D_PERROR( "FluxComp: fopen( '%s' ) failed!\n", filename.c_str() );
          return;
     }

     fprintf( file, "/*\n"
                    " *	This file was automatically generated by fluxcomp; DO NOT EDIT!\n"
                    " */\n"
                    "%s\n"
                    "#include <config.h>\n"
                    "\n"
                    "#include \"%s.h\"\n"
                    "\n",
              license, face->object.c_str() );

     if (!config.c_mode)
          fprintf( file, "extern \"C\" {\n" );

     fprintf( file, "#include <directfb_util.h>\n"
                    "\n"
                    "#include <direct/debug.h>\n"
                    "#include <direct/mem.h>\n"
                    "#include <direct/memcpy.h>\n"
                    "#include <direct/messages.h>\n"
                    "\n"
                    "#include <fusion/conf.h>\n"
                    "\n"
                    "#include <core/core.h>\n" );

     if (config.call_mode)
          fprintf( file, "\n"
                         "#include <core/CoreDFB_CallMode.h>\n" );

     if (!config.c_mode)
          fprintf( file, "}\n" );

     fprintf( file, "\n"
                    "D_DEBUG_DOMAIN( DirectFB_%s, \"DirectFB/%s\", \"DirectFB %s\" );\n"
                    "\n"
                    "/*********************************************************************************************************************/\n"
                    "\n",
              face->object.c_str(), face->object.c_str(), face->object.c_str() );

     /* C Wrappers */

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          if (!config.c_mode) {
               fprintf( file, "DFBResult\n"
                              "%s_%s(\n"
                              "                    %-40s  *obj%s\n"
                              "%s\n"
                              ")\n"
                              "{\n",
                        face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), method->entities.empty() ? "" : ",",
                        method->ArgumentsAsParamDecl().c_str() );

               if (config.call_mode) {
                    fprintf( file,      "    DFBResult ret;\n"
                                        "\n"
                                        "    switch (CoreDFB_CallMode( core_dfb )) {\n"
                                        "        case COREDFB_CALL_DIRECT:" );
                    if (direct)
                         fprintf( file, "{\n"
                                        "            DirectFB::%s_Real real( core_dfb, obj );\n"
                                        "\n"
                                        "            Core_PushCalling();\n"
                                        "            ret = real.%s( %s );\n"
                                        "            Core_PopCalling();\n"                                  
                                        "\n"
                                        "            return ret;\n"
                                        "        }",
                             face->name.c_str(),
                             method->name.c_str(), method->ArgumentsNames().c_str() );

                    fprintf( file,      "\n        case COREDFB_CALL_INDIRECT: {\n"
                                        "            DirectFB::%s_Requestor requestor( core_dfb, obj );\n"
                                        "\n"
                                        "            Core_PushCalling();\n"
                                        "            ret = requestor.%s( %s );\n"
                                        "            Core_PopCalling();\n"                                  
                                        "\n"
                                        "            return ret;\n"
                                        "        }\n"
                                        "        case COREDFB_CALL_DENY:\n"
                                        "            return DFB_DEAD;\n"
                                        "    }\n"
                                        "\n"
                                        "    return DFB_UNIMPLEMENTED;\n"
                                        "}\n"
                                        "\n",
                             face->name.c_str(),
                             method->name.c_str(), method->ArgumentsNames().c_str() );
               }
               else {
                    if (direct)
                         fprintf( file, "    if (!fusion_config->secure_fusion || dfb_core_is_master( core_dfb )) {\n"
                                        "        DirectFB::%s_Real real( core_dfb, obj );\n"
                                        "\n"
                                        "        return real.%s( %s );\n"
                                        "    }\n"
                                        "\n",
                                  face->name.c_str(),
                                  method->name.c_str(), method->ArgumentsNames().c_str() );

                    fprintf( file,      "    DirectFB::%s_Requestor requestor( core_dfb, obj );\n"
                                        "\n"
                                        "    return requestor.%s( %s );\n"
                                        "}\n"
                                        "\n",
                             face->name.c_str(),
                             method->name.c_str(), method->ArgumentsNames().c_str() );

               }
          }
          else {
               fprintf( file, "DFBResult\n"
                              "%s_%s(\n"
                              "                    %-40s  *obj%s\n"
                              "%s\n"
                              ")\n"
                              "{\n",
                        face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), method->entities.empty() ? "" : ",",
                        method->ArgumentsAsParamDecl().c_str() );

               if (config.call_mode) {
                    fprintf( file,      "    DFBResult ret;\n"
                                        "\n"                                        
                                        "    switch (CoreDFB_CallMode( core_dfb )) {\n"
                                        "        case COREDFB_CALL_DIRECT:" );
                    if (direct)
                         fprintf( file, "{\n"
                                        "            Core_PushCalling();\n"
                                        "            ret = %s_Real__%s( obj%s%s );\n"
                                        "            Core_PopCalling();\n"
                                        "\n"
                                        "            return ret;\n"
                                        "        }\n",
                                  face->name.c_str(), method->name.c_str(), method->ArgumentsAsParamDecl().empty() ? "" : ", ", method->ArgumentsNames().c_str() );

                    fprintf( file,      "\n        case COREDFB_CALL_INDIRECT: {\n"
                                        "            Core_PushCalling();\n"
                                        "            ret = %s_Requestor__%s( obj%s%s );\n"
                                        "            Core_PopCalling();\n"
                                        "\n"
                                        "            return ret;\n"
                                        "        }\n"
                                        "        case COREDFB_CALL_DENY:\n"
                                        "            return DFB_DEAD;\n"
                                        "    }\n"
                                        "\n"
                                        "    return DFB_UNIMPLEMENTED;\n"
                                        "}\n"
                                        "\n",
                             face->name.c_str(), method->name.c_str(), method->ArgumentsAsParamDecl().empty() ? "" : ", ", method->ArgumentsNames().c_str() );
               }
               else {
                    if (direct)
                         fprintf( file, "    if (!fusion_config->secure_fusion || dfb_core_is_master( core_dfb )) {\n"
                                        "        return %s_Real__%s( obj%s%s );\n"
                                        "    }\n"
                                        "\n",
                                  face->name.c_str(), method->name.c_str(), method->ArgumentsAsParamDecl().empty() ? "" : ", ", method->ArgumentsNames().c_str() );

                    fprintf( file,      "    return %s_Requestor__%s( obj%s%s );\n"
                                        "}\n"
                                        "\n",
                             face->name.c_str(), method->name.c_str(), method->ArgumentsAsParamDecl().empty() ? "" : ", ", method->ArgumentsNames().c_str() );
               }
          }
     }


     fprintf( file, "/*********************************************************************************************************************/\n"
                    "\n"
                    "static FusionCallHandlerResult\n"
                    "%s_Dispatch( int           caller,   /* fusion id of the caller */\n"
                    "                     int           call_arg, /* optional call parameter */\n"
                    "                     void         *ptr, /* optional call parameter */\n"
                    "                     unsigned int  length,\n"
                    "                     void         *ctx,      /* optional handler context */\n"
                    "                     unsigned int  serial,\n"
                    "                     void         *ret_ptr,\n"
                    "                     unsigned int  ret_size,\n"
                    "                     unsigned int *ret_length )\n"
                    "{\n",
              face->object.c_str() );

     fprintf( file, "    %s *obj = (%s*) ctx;"
                    "\n"
                    "    %s%sDispatch__Dispatch( obj, caller, call_arg, ptr, length, ret_ptr, ret_size, ret_length );\n"
                    "\n"
                    "    return FCHR_RETURN;\n"
                    "}\n"
                    "\n",
              face->dispatch.c_str(), face->dispatch.c_str(),
              config.c_mode ? "" : "DirectFB::", face->object.c_str() );


     fprintf( file, "void %s_Init_Dispatch(\n"
                    "                    %-20s *core,\n"
                    "                    %-20s *obj,\n"
                    "                    FusionCall           *call\n"
                    ")\n"
                    "{\n",
                    face->object.c_str(), face->core.c_str(), face->dispatch.c_str() );

     fprintf( file, "    fusion_call_init3( call, %s_Dispatch, obj, core->world );\n"
                    "}\n"
                    "\n",
              face->object.c_str() );

     fprintf( file, "void  %s_Deinit_Dispatch(\n"
                    "                    FusionCall           *call\n"
                    ")\n"
                    "{\n"
                    "     fusion_call_destroy( call );\n"
                    "}\n"
                    "\n",
              face->object.c_str() );

     fprintf( file, "/*********************************************************************************************************************/\n"
                    "\n" );

     if (!config.c_mode) {
          fprintf( file, "namespace DirectFB {\n"
                         "\n"
                         "\n" );
     }

     /* Requestor Methods */
     fprintf( file, "static __inline__ void *args_alloc( void *static_buffer, size_t size )\n"
                    "{\n"
                    "    void *buffer = static_buffer;\n"
                    "\n"
                    "    if (size > %s) {\n"
                    "        buffer = D_MALLOC( size );\n"
                    "        if (!buffer)\n"
                    "            return NULL;\n"
                    "    }\n"
                    "\n"
                    "    return buffer;\n"
                    "}\n"
                    "\n",
              config.static_args_bytes.c_str() );

     fprintf( file, "static __inline__ void args_free( void *static_buffer, void *buffer )\n"
                    "{\n"
                    "    if (buffer != static_buffer)\n"
                    "        D_FREE( buffer );\n"
                    "}\n"
                    "\n" );

     if (face->buffered) {
          fprintf( file, "\n"
                         "DFBResult\n"
                         "%s_Requestor::flushCalls()\n"
                         "{\n"
                         "     DFBResult ret;\n"
                         "\n"
                         "     ret = (DFBResult) %s_Call( obj, (FusionCallExecFlags)(FCEF_ONEWAY | FCEF_QUEUE), -1, buffer, buffer_len, NULL, 0, NULL );\n"
                         "     if (ret) {\n"
                         "         D_DERROR( ret, \"%%s: %s_Call( -1 ) failed!\\n\", __FUNCTION__ );\n"
                         "         return ret;\n"
                         "     }\n"
                         "\n"
                         "     return DFB_OK;\n"
                         "}\n",
                   face->name.c_str(), face->object.c_str(), face->object.c_str() );
     }

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          if (!config.c_mode) {
               fprintf( file, "\n"
                              "DFBResult\n"
                              "%s_Requestor::%s(\n"
                              "%s\n"
                              ")\n",
                        face->name.c_str(), method->name.c_str(),
                        method->ArgumentsAsParamDecl().c_str() );
          }
          else {
               fprintf( file, "\n"
                              "DFBResult\n"
                              "%s_Requestor__%s( %s *obj%s\n"
                              "%s\n"
                              ")\n",
                        face->name.c_str(), method->name.c_str(), face->object.c_str(), method->ArgumentsAsParamDecl().empty() ? "" : ",",
                        method->ArgumentsAsParamDecl().c_str() );

          }

          if (method->buffer && face->buffered) {
               fprintf( file, "{\n"
                              "%s"
                              "    %s%s *args = (%s%s*) prepare( %s%s_%s, %s );\n"
                              "\n"
                              "    if (!args)\n"
                              "        return (DFBResult) D_OOM();\n"
                              "\n"
                              "    D_DEBUG_AT( DirectFB_%s, \"%s_Requestor::%%s()\\n\", __FUNCTION__ );\n"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "\n"
                              "    commit();\n"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "%s"
                              "\n"
                              "    return DFB_OK;\n"
                              "}\n"
                              "\n",
                        method->OpenSplitArgumentsIfRequired().c_str(),
                        face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), method->name.c_str(),
                        config.c_mode ? "_" : "", face->object.c_str(), method->name.c_str(),
                        method->ArgumentsSize( face, false ).c_str(),
                        face->object.c_str(), face->name.c_str(),
                        method->ArgumentsAssertions().c_str(),
                        method->ArgumentsInputAssignments().c_str(),
                        method->ArgumentsOutputAssignments().c_str(),
                        method->ArgumentsOutputObjectCatch( config ).c_str(),
                        method->CloseSplitArgumentsIfRequired().c_str() );
          }
          else if (method->async) {
               fprintf( file, "{\n"
                              "    DFBResult           ret = DFB_OK;\n"
                              "%s"
                              "    char        args_static[%s];\n"
                              "    %s%s       *args = (%s%s*) args_alloc( args_static, %s );\n"
                              "\n"
                              "    if (!args)\n"
                              "        return (DFBResult) D_OOM();\n"
                              "\n"
                              "    D_DEBUG_AT( DirectFB_%s, \"%s_Requestor::%%s()\\n\", __FUNCTION__ );\n"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "    ret = (DFBResult) %s_Call( obj, (FusionCallExecFlags)(FCEF_ONEWAY%s), %s%s_%s, args, %s, NULL, 0, NULL );\n"
                              "    if (ret) {\n"
                              "        D_DERROR( ret, \"%%s: %s_Call( %s_%s ) failed!\\n\", __FUNCTION__ );\n"
                              "        goto out;\n"
                              "    }\n"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "\n"
                              "out:\n"
                              "    args_free( args_static, args );\n"
                              "    return ret;\n"
                              "}\n"
                              "\n",
                        method->ArgumentsOutputObjectDecl().c_str(),
                        config.static_args_bytes.c_str(),
                        face->object.c_str(), method->name.c_str(), face->object.c_str(), method->name.c_str(), method->ArgumentsSize( face, false ).c_str(),
                        face->object.c_str(), face->name.c_str(),
                        method->ArgumentsAssertions().c_str(),
                        method->ArgumentsInputAssignments().c_str(),
                        face->buffered ? "    flush();\n\n" : "",
                        face->object.c_str(), method->queue ? " | FCEF_QUEUE" : "", config.c_mode ? "_" : "", face->object.c_str(), method->name.c_str(), method->ArgumentsSize( face, false ).c_str(),
                        face->object.c_str(), face->object.c_str(), method->name.c_str(),
                        method->ArgumentsOutputAssignments().c_str(),
                        method->ArgumentsOutputObjectCatch( config ).c_str() );
          }
          else {
               fprintf( file, "{\n"
                              "    DFBResult           ret = DFB_OK;\n"
                              "%s"
                              "    char        args_static[%s];\n"
                              "    char        return_args_static[%s];\n"
                              "    %s%s       *args = (%s%s*) args_alloc( args_static, %s );\n"
                              "    %s%sReturn *return_args;\n"
                              "\n"
                              "    if (!args)\n"
                              "        return (DFBResult) D_OOM();\n"
                              "\n"
                              "    return_args = (%s%sReturn*) args_alloc( return_args_static, %s );\n"
                              "\n"
                              "    if (!return_args) {\n"
                              "        args_free( args_static, args );\n"
                              "        return (DFBResult) D_OOM();\n"
                              "    }\n"
                              "\n"
                              "    D_DEBUG_AT( DirectFB_%s, \"%s_Requestor::%%s()\\n\", __FUNCTION__ );\n"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "    ret = (DFBResult) %s_Call( obj, FCEF_NONE, %s%s_%s, args, %s, return_args, %s, NULL );\n"
                              "    if (ret) {\n"
                              "        D_DERROR( ret, \"%%s: %s_Call( %s_%s ) failed!\\n\", __FUNCTION__ );\n"
                              "        goto out;\n"
                              "    }\n"
                              "\n"
                              "    if (return_args->result) {\n"
                              "        /*D_DERROR( return_args->result, \"%%s: %s_%s failed!\\n\", __FUNCTION__ );*/\n"
                              "        ret = return_args->result;\n"
                              "        goto out;\n"
                              "    }\n"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "\n"
                              "out:\n"
                              "    args_free( return_args_static, return_args );\n"
                              "    args_free( args_static, args );\n"
                              "    return ret;\n"
                              "}\n"
                              "\n",
                        method->ArgumentsOutputObjectDecl().c_str(),
                        config.static_args_bytes.c_str(),
                        config.static_args_bytes.c_str(),
                        face->object.c_str(), method->name.c_str(), face->object.c_str(), method->name.c_str(), method->ArgumentsSize( face, false ).c_str(),
                        face->object.c_str(), method->name.c_str(), face->object.c_str(), method->name.c_str(), method->ArgumentsSize( face, true ).c_str(),
                        face->object.c_str(), face->name.c_str(),
                        method->ArgumentsAssertions().c_str(),
                        method->ArgumentsInputAssignments().c_str(),
                        face->buffered ? "     flush();\n\n" : "",
                        face->object.c_str(), config.c_mode ? "_" : "", face->object.c_str(), method->name.c_str(), method->ArgumentsSize( face, false ).c_str(), method->ArgumentsSize( face, true ).c_str(),
                        face->object.c_str(), face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), method->name.c_str(),
                        method->ArgumentsOutputAssignments().c_str(),
                        method->ArgumentsOutputObjectCatch( config ).c_str() );
          }
     }

     /* Dispatch Object */

     fprintf( file, "/*********************************************************************************************************************/\n"
                    "\n"
                    "static DFBResult\n"
                    "__%sDispatch__Dispatch( %s *obj,\n"
                    "                                FusionID      caller,\n"
                    "                                int           method,\n"
                    "                                void         *ptr,\n"
                    "                                unsigned int  length,\n"
                    "                                void         *ret_ptr,\n"
                    "                                unsigned int  ret_size,\n"
                    "                                unsigned int *ret_length )\n"
                    "{\n"
                    "    D_UNUSED\n"
                    "    DFBResult ret;\n"
                    "\n",
              face->object.c_str(), face->dispatch.c_str() );

     if (!config.c_mode)
          fprintf( file, "\n"
                         "    DirectFB::%s_Real real( core_dfb, obj );\n"
                         "\n",
                   face->name.c_str() );

     fprintf( file, "\n"
                    "    switch (method) {\n" );

     /* Dispatch Methods */

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          if (!config.c_mode)
               fprintf( file, "        case %s_%s: {\n", face->object.c_str(), method->name.c_str() );
          else
               fprintf( file, "        case _%s_%s: {\n", face->object.c_str(), method->name.c_str() );

          if (method->async) {
               fprintf( file, "%s"
                              "%s"
                              "            D_UNUSED\n"
                              "            %s%s       *args        = (%s%s *) ptr;\n"
                              "\n"
                              "            D_DEBUG_AT( DirectFB_%s, \"=-> %s_%s\\n\" );\n"
                              "\n"
                              "%s"
                              "\n"
                              "%s",
                        method->ArgumentsInputObjectDecl().c_str(),
                        method->ArgumentsOutputObjectDecl().c_str(),
                        face->object.c_str(), method->name.c_str(), face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), face->object.c_str(), method->name.c_str(),
                        method->ArgumentsInputDebug( config, face ).c_str(),
                        method->ArgumentsInputObjectLookup( config ).c_str() );

               if (!config.c_mode)
                    fprintf( file, "            real.%s( %s );\n",
                             method->name.c_str(), method->ArgumentsAsMemberParams().c_str() );
               else
                    fprintf( file, "            %s_Real__%s( obj%s%s );\n",
                             face->name.c_str(), method->name.c_str(), method->ArgumentsAsParamDecl().empty() ? "" : ", ", method->ArgumentsAsMemberParams().c_str() );

               fprintf( file, "\n"
                              "%s"
                              "            return DFB_OK;\n"
                              "        }\n"
                              "\n",
                        method->ArgumentsInputObjectUnref().c_str() );
          }
          else {
               fprintf( file, "%s"
                              "%s"
                              "%s"
                              "            D_UNUSED\n"
                              "            %s%s       *args        = (%s%s *) ptr;\n"
                              "            %s%sReturn *return_args = (%s%sReturn *) ret_ptr;\n"
                              "\n"
                              "            D_DEBUG_AT( DirectFB_%s, \"=-> %s_%s\\n\" );\n"
                              "\n"
                              "%s",
                        method->ArgumentsInputObjectDecl().c_str(),
                        method->ArgumentsOutputObjectDecl().c_str(),
                        method->ArgumentsOutputTmpDecl().c_str(),
                        face->object.c_str(), method->name.c_str(), face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), method->name.c_str(), face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), face->object.c_str(), method->name.c_str(),
                        method->ArgumentsInputObjectLookup( config ).c_str() );

               if (!config.c_mode)
                    fprintf( file, "            return_args->result = real.%s( %s );\n",
                             method->name.c_str(), method->ArgumentsAsMemberParams().c_str() );
               else
                    fprintf( file, "            return_args->result = %s_Real__%s( obj%s%s );\n",
                             face->name.c_str(), method->name.c_str(), method->ArgumentsAsParamDecl().empty() ? "" : ", ", method->ArgumentsAsMemberParams().c_str() );


               fprintf( file, "            if (return_args->result == DFB_OK) {\n"
                              "%s"
                              "%s"
                              "%s"
                              "            }\n"
                              "\n"
                              "            *ret_length = %s;\n"
                              "\n"
                              "%s"
                              "            return DFB_OK;\n"
                              "        }\n"
                              "\n",
                        method->ArgumentsOutputObjectThrow( config ).c_str(),
                        method->ArgumentsInoutReturn().c_str(),
                        method->ArgumentsOutputTmpReturn().c_str(),
                        method->ArgumentsSizeReturn( face ).c_str(),
                        method->ArgumentsInputObjectUnref().c_str() );
          }
     }

     fprintf( file, "    }\n"
                    "\n"
                    "    return DFB_NOSUCHMETHOD;\n"
                    "}\n" );


     fprintf( file, "/*********************************************************************************************************************/\n"
                    "\n"
                    "DFBResult\n"
                    "%sDispatch__Dispatch( %s *obj,\n"
                    "                                FusionID      caller,\n"
                    "                                int           method,\n"
                    "                                void         *ptr,\n"
                    "                                unsigned int  length,\n"
                    "                                void         *ret_ptr,\n"
                    "                                unsigned int  ret_size,\n"
                    "                                unsigned int *ret_length )\n"
                    "{\n"
                    "    DFBResult ret = DFB_OK;\n"
                    "\n"
                    "    D_DEBUG_AT( DirectFB_%s, \"%sDispatch::%%s( %%p )\\n\", __FUNCTION__, obj );\n",
              face->object.c_str(), face->dispatch.c_str(),
              face->object.c_str(), face->object.c_str());

     if (config.identity)
          fprintf( file, "\n"
                         "    Core_PushIdentity( caller );\n" );

     if (face->buffered) {
          fprintf( file, "\n"
                         "    if (method == -1) {\n"
                         "        unsigned int consumed = 0;\n"
                         "\n"
                         "        while (consumed < length) {\n"
                         "            u32 *p = (u32*)( (u8*) ptr + consumed );\n"
                         "\n"
                         "            if ((p[0] > length - consumed) || (consumed + p[0] > length)) {\n"
                         "                D_WARN( \"invalid data from caller %%lu\", caller );\n"
                         "                break;\n"
                         "            }\n"
                         "\n"
                         "            consumed += p[0];\n"
                         "\n"
                         "            ret = __%sDispatch__Dispatch( obj, caller, p[1], p + 2, p[0] - sizeof(int) * 2, NULL, 0, NULL );\n"
                         "            if (ret)\n"
                         "                break;\n"
                         "        }\n"
                         "    }\n"
                         "    else\n",
                   face->object.c_str() );
     }

     fprintf( file, "\n"
                    "    ret = __%sDispatch__Dispatch( obj, caller, method, ptr, length, ret_ptr, ret_size, ret_length );\n",
              face->object.c_str() );

     if (config.identity)
          fprintf( file, "\n"
                         "    Core_PopIdentity();\n" );

     fprintf( file, "\n"
                    "    return ret;\n"
                    "}\n" );

     if (!config.c_mode)
          fprintf( file, "\n"
                         "}\n" );

     fclose( file );
}

void
FluxComp::PrintInterface( FILE              *file,
                          const Interface   *face,
                          const std::string &name,
                          const std::string &super,
                          bool               abstract )
{
     fprintf( file, "\n"
                    "\n"
                    "\n"
                    "class %s : public %s\n"
                    "{\n"
                    "public:\n"
                    "    %s( %s *core )\n"
                    "        :\n"
                    "        %s( core )\n"
                    "    {\n"
                    "    }\n"
                    "\n"
                    "public:\n",
              name.c_str(), super.c_str(), name.c_str(), face->core.c_str(), super.c_str() );

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          fprintf( file, "    virtual DFBResult %s(\n"
                         "%s\n"
                         "    )%s;\n"
                         "\n",
                   method->name.c_str(),
                   method->ArgumentsAsParamDecl().c_str(),
                   abstract ? " = 0" : "" );
     }

     fprintf( file, "};\n" );
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DirectResult   ret;
     Entity::vector faces;

     FluxConfig     config;

     direct_initialize();

//     direct_debug_config_domain( "fluxcomp", true );

//     direct_config->debug    = true;
//     direct_config->debugmem = true;

     /* Parse the command line. */
     if (!config.parse_command_line( argc, argv ))
          return -1;

     ret = Entity::GetEntities( filename, faces );
     if (ret == DR_OK) {
          FluxComp fc;

          for (Entity::vector::const_iterator iter = faces.begin(); iter != faces.end(); iter++) {
               Interface *face = dynamic_cast<Interface*>( *iter );

               for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
                    Method *method = dynamic_cast<Method*>( *iter );

                    if (!method->async || !method->queue)
                         method->buffer = false;

                    if (method->buffer) {
                         face->buffered = true;
                         break;
                    }
               }

               fc.GenerateHeader( face, config );
               fc.GenerateSource( face, config );
          }
     }


     direct_print_memleaks();

     direct_shutdown();

     return ret;
}

