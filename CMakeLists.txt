cmake_minimum_required(VERSION 3.14)

# UTF-8 source
add_compile_options("$<$<AND:$<C_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C,CXX>>:/utf-8>")
add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/Zc:__cplusplus>")

project(thorvg
    VERSION 1.0.0
)

# C++14 required
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(TVG_BUILD_SHARED "Build shared library" ON)
option(TVG_THREADS "Enable multi-threading task scheduler" ON)
option(TVG_PARTIAL "Enable Partial Rendering" ON)
option(TVG_SIMD "Enable CPU Vectorization (SIMD)" OFF)
option(TVG_LOG "Enable log message" OFF)
option(TVG_FILE_IO "Enable File IO calls" ON)

# Engine options
option(TVG_ENGINE_SW "Enable Software Rasterizer Engine" ON)
option(TVG_ENGINE_GL "Enable OpenGL Rasterizer Engine" OFF)
option(TVG_ENGINE_WG "Enable WebGPU Rasterizer Engine" OFF)

# Loader options
option(TVG_LOADER_SVG "Enable SVG Loader" ON)
option(TVG_LOADER_PNG "Enable PNG Loader" OFF)
option(TVG_LOADER_JPG "Enable JPG Loader" OFF)
option(TVG_LOADER_LOTTIE "Enable Lottie Loader" ON)
option(TVG_LOADER_TTF "Enable TTF Loader" ON)
option(TVG_LOADER_WEBP "Enable WebP Loader" OFF)

# Saver options
option(TVG_SAVER_GIF "Enable GIF Saver" OFF)

# Binding options
option(TVG_BINDINGS_CAPI "Enable C API bindings" OFF)

# Tool options
option(TVG_TOOL_SVG2PNG "Build svg2png tool" OFF)
option(TVG_TOOL_LOTTIE2GIF "Build lottie2gif tool" OFF)

# Extra options
option(TVG_LOTTIE_EXPRESSIONS "Enable Lottie Expressions support" ON)
option(TVG_OPENMP "Enable OpenMP support" ON)
option(TVG_OPENGL_ES "Use OpenGL ES instead of OpenGL" OFF)
option(TVG_GL_INITPROC "Use user-provided GL function loader" OFF)

# Test options
option(TVG_BUILD_TESTS "Build unit tests" OFF)

# Use static linking modules
option(TVG_STATIC_MODULES "Force to use static linking modules" OFF)

# Set library type
if(TVG_BUILD_SHARED)
    set(TVG_LIB_TYPE SHARED)
else()
    set(TVG_LIB_TYPE STATIC)
endif()

# Version variables
set(THORVG_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(THORVG_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(THORVG_VERSION_MICRO ${PROJECT_VERSION_PATCH})

# Config variables for config.h (only set if enabled, for #cmakedefine)
if(TVG_THREADS)
    set(THORVG_THREAD_SUPPORT 1)
endif()

if(TVG_ENGINE_SW)
    set(THORVG_SW_RASTER_SUPPORT 1)
endif()

if(TVG_ENGINE_GL)
    set(THORVG_GL_RASTER_SUPPORT 1)
endif()

if(TVG_ENGINE_WG)
    set(THORVG_WG_RASTER_SUPPORT 1)
endif()

if(TVG_PARTIAL)
    set(THORVG_PARTIAL_RENDER_SUPPORT 1)
endif()

if(TVG_LOADER_SVG OR TVG_TOOL_SVG2PNG)
    set(THORVG_SVG_LOADER_SUPPORT 1)
endif()

if(TVG_LOADER_PNG)
    set(THORVG_PNG_LOADER_SUPPORT 1)
endif()

if(TVG_LOADER_JPG)
    set(THORVG_JPG_LOADER_SUPPORT 1)
endif()

if(TVG_LOADER_LOTTIE OR TVG_TOOL_LOTTIE2GIF)
    set(THORVG_LOTTIE_LOADER_SUPPORT 1)
endif()

if(TVG_LOADER_TTF)
    set(THORVG_TTF_LOADER_SUPPORT 1)
endif()

if(TVG_LOADER_WEBP)
    set(THORVG_WEBP_LOADER_SUPPORT 1)
endif()

if(TVG_SAVER_GIF OR TVG_TOOL_LOTTIE2GIF)
    set(THORVG_GIF_SAVER_SUPPORT 1)
endif()

# SIMD support detection
if(TVG_SIMD)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
        set(THORVG_AVX_VECTOR_SUPPORT 1)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
        set(THORVG_NEON_VECTOR_SUPPORT 1)
    endif()
endif()

if(TVG_BINDINGS_CAPI)
    set(THORVG_CAPI_BINDING_SUPPORT 1)
endif()

if(TVG_LOG)
    set(THORVG_LOG_ENABLED 1)
endif()

if(TVG_FILE_IO)
    set(THORVG_FILE_IO_SUPPORT 1)
endif()

if(TVG_LOTTIE_EXPRESSIONS AND (TVG_LOADER_LOTTIE OR TVG_TOOL_LOTTIE2GIF))
    set(THORVG_LOTTIE_EXPRESSIONS_SUPPORT 1)
endif()

if(TVG_OPENMP)
    set(THORVG_OPENMP_SUPPORT 1)
endif()

# Generate config.h
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    @ONLY
)

# Include directories
set(THORVG_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Compiler flags
set(THORVG_COMPILE_FLAGS "")
set(THORVG_PRIVATE_COMPILE_FLAGS "")

if(TVG_BUILD_SHARED)
    list(APPEND THORVG_PRIVATE_COMPILE_FLAGS -DTVG_EXPORT -DTVG_BUILD)
else()
    list(APPEND THORVG_COMPILE_FLAGS -DTVG_STATIC)
endif()

if(MSVC)
    list(APPEND THORVG_COMPILE_FLAGS -DNOMINMAX)
else()
    list(APPEND THORVG_PRIVATE_COMPILE_FLAGS -Wno-unknown-pragmas)
    if(NOT TVG_LOG)
        list(APPEND THORVG_PRIVATE_COMPILE_FLAGS -Wdouble-promotion)
    endif()
    list(APPEND THORVG_PRIVATE_COMPILE_FLAGS 
        -fno-exceptions 
        -fno-rtti 
        -fno-stack-protector 
        -fno-math-errno
        -fno-unwind-tables 
        -fno-asynchronous-unwind-tables 
        -Woverloaded-virtual
    )
endif()

# SIMD support
if(TVG_SIMD)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
        if(MSVC)
            list(APPEND THORVG_PRIVATE_COMPILE_FLAGS /arch:AVX)
        else()
            list(APPEND THORVG_PRIVATE_COMPILE_FLAGS -mavx)
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
        if(NOT MSVC)
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
                list(APPEND THORVG_PRIVATE_COMPILE_FLAGS -mfpu=neon)
            endif()
        endif()
    endif()
endif()

# OpenGL variant
if(TVG_ENGINE_GL)
    if(TVG_OPENGL_ES)
        list(APPEND THORVG_PRIVATE_COMPILE_FLAGS -DTHORVG_GL_TARGET_GLES=1)
    else()
        list(APPEND THORVG_PRIVATE_COMPILE_FLAGS -DTHORVG_GL_TARGET_GL=1)
    endif()
    if(TVG_GL_INITPROC)
        list(APPEND THORVG_PRIVATE_COMPILE_FLAGS -DTHORVG_GL_INITPROC=1)
    endif()
endif()

# Source files collection
set(THORVG_SOURCES "")
set(THORVG_PRIVATE_INCLUDE_DIRS "")

# Common sources
list(APPEND THORVG_SOURCES
    src/common/tvgColor.cpp
    src/common/tvgCompressor.cpp
    src/common/tvgMath.cpp
    src/common/tvgStr.cpp
)
list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/common)

# Renderer sources
list(APPEND THORVG_SOURCES
    src/renderer/tvgAccessor.cpp
    src/renderer/tvgAnimation.cpp
    src/renderer/tvgCanvas.cpp
    src/renderer/tvgFill.cpp
    src/renderer/tvgInitializer.cpp
    src/renderer/tvgLoader.cpp
    src/renderer/tvgPaint.cpp
    src/renderer/tvgPicture.cpp
    src/renderer/tvgRender.cpp
    src/renderer/tvgSaver.cpp
    src/renderer/tvgScene.cpp
    src/renderer/tvgShape.cpp
    src/renderer/tvgTaskScheduler.cpp
    src/renderer/tvgText.cpp
)
list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer)

# SW Engine
if(TVG_ENGINE_SW)
    list(APPEND THORVG_SOURCES
        src/renderer/sw_engine/tvgSwFill.cpp
        src/renderer/sw_engine/tvgSwImage.cpp
        src/renderer/sw_engine/tvgSwMath.cpp
        src/renderer/sw_engine/tvgSwMemPool.cpp
        src/renderer/sw_engine/tvgSwPostEffect.cpp
        src/renderer/sw_engine/tvgSwRaster.cpp
        src/renderer/sw_engine/tvgSwRenderer.cpp
        src/renderer/sw_engine/tvgSwRle.cpp
        src/renderer/sw_engine/tvgSwShape.cpp
        src/renderer/sw_engine/tvgSwStroke.cpp
    )
    list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/sw_engine)
endif()

# GL Engine
if(TVG_ENGINE_GL)
    list(APPEND THORVG_SOURCES
        src/renderer/gl_engine/tvgGl.cpp
        src/renderer/gl_engine/tvgGlEffect.cpp
        src/renderer/gl_engine/tvgGlGeometry.cpp
        src/renderer/gl_engine/tvgGlGpuBuffer.cpp
        src/renderer/gl_engine/tvgGlProgram.cpp
        src/renderer/gl_engine/tvgGlRenderer.cpp
        src/renderer/gl_engine/tvgGlRenderPass.cpp
        src/renderer/gl_engine/tvgGlRenderTarget.cpp
        src/renderer/gl_engine/tvgGlRenderTask.cpp
        src/renderer/gl_engine/tvgGlShader.cpp
        src/renderer/gl_engine/tvgGlShaderSrc.cpp
        src/renderer/gl_engine/tvgGlTessellator.cpp
    )
endif()
# Always include GL engine headers for conditional compilation
list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/gl_engine)

# WG Engine
if(TVG_ENGINE_WG)
    list(APPEND THORVG_SOURCES
        src/renderer/wg_engine/tvgWgBindGroups.cpp
        src/renderer/wg_engine/tvgWgCommon.cpp
        src/renderer/wg_engine/tvgWgCompositor.cpp
        src/renderer/wg_engine/tvgWgGeometry.cpp
        src/renderer/wg_engine/tvgWgPipelines.cpp
        src/renderer/wg_engine/tvgWgRenderData.cpp
        src/renderer/wg_engine/tvgWgRenderer.cpp
        src/renderer/wg_engine/tvgWgRenderTarget.cpp
        src/renderer/wg_engine/tvgWgRenderTask.cpp
        src/renderer/wg_engine/tvgWgShaderSrc.cpp
        src/renderer/wg_engine/tvgWgShaderTypes.cpp
        src/renderer/wg_engine/tvgWgTessellator.cpp
    )
    # Only include WG engine headers when building with WG engine (requires webgpu headers)
    list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/wg_engine)
    
    # Find wgpu_native
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(WGPU wgpu_native)
    endif()
endif()

# Loader include directory
list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders)

# SVG Loader
if(TVG_LOADER_SVG OR TVG_TOOL_SVG2PNG)
    list(APPEND THORVG_SOURCES
        src/loaders/svg/tvgSvgCssStyle.cpp
        src/loaders/svg/tvgSvgLoader.cpp
        src/loaders/svg/tvgSvgPath.cpp
        src/loaders/svg/tvgSvgSceneBuilder.cpp
        src/loaders/svg/tvgSvgUtil.cpp
        src/loaders/svg/tvgXmlParser.cpp
    )
    list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/svg)
endif()

# TTF Loader
if(TVG_LOADER_TTF)
    list(APPEND THORVG_SOURCES
        src/loaders/ttf/tvgTtfLoader.cpp
        src/loaders/ttf/tvgTtfReader.cpp
    )
    list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/ttf)
endif()

# Lottie Loader
if(TVG_LOADER_LOTTIE OR TVG_TOOL_LOTTIE2GIF)
    list(APPEND THORVG_SOURCES
        src/loaders/lottie/tvgLottieAnimation.cpp
        src/loaders/lottie/tvgLottieBuilder.cpp
        src/loaders/lottie/tvgLottieExpressions.cpp
        src/loaders/lottie/tvgLottieInterpolator.cpp
        src/loaders/lottie/tvgLottieLoader.cpp
        src/loaders/lottie/tvgLottieModel.cpp
        src/loaders/lottie/tvgLottieModifier.cpp
        src/loaders/lottie/tvgLottieParserHandler.cpp
        src/loaders/lottie/tvgLottieParser.cpp
    )
    list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/lottie)
    
    # JerryScript for Lottie Expressions
    if(THORVG_LOTTIE_EXPRESSIONS_SUPPORT)
        # JerryScript sources
        set(JERRYSCRIPT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/lottie/jerryscript/jerry-core)
        
        list(APPEND THORVG_SOURCES
            # API
            ${JERRYSCRIPT_DIR}/api/jerryscript.cpp
            # jcontext
            ${JERRYSCRIPT_DIR}/jcontext/jcontext.cpp
            # jmem
            ${JERRYSCRIPT_DIR}/jmem/jmem-allocator.cpp
            ${JERRYSCRIPT_DIR}/jmem/jmem-heap.cpp
            ${JERRYSCRIPT_DIR}/jmem/jmem-poolman.cpp
            # jrt
            ${JERRYSCRIPT_DIR}/jrt/jrt-fatals.cpp
            ${JERRYSCRIPT_DIR}/jrt/jrt-logging.cpp
            # lit
            ${JERRYSCRIPT_DIR}/lit/lit-char-helpers.cpp
            ${JERRYSCRIPT_DIR}/lit/lit-magic-strings.cpp
            ${JERRYSCRIPT_DIR}/lit/lit-strings.cpp
            # parser/js
            ${JERRYSCRIPT_DIR}/parser/js/byte-code.cpp
            ${JERRYSCRIPT_DIR}/parser/js/common.cpp
            ${JERRYSCRIPT_DIR}/parser/js/js-lexer.cpp
            ${JERRYSCRIPT_DIR}/parser/js/js-parser.cpp
            ${JERRYSCRIPT_DIR}/parser/js/js-parser-expr.cpp
            ${JERRYSCRIPT_DIR}/parser/js/js-parser-mem.cpp
            ${JERRYSCRIPT_DIR}/parser/js/js-parser-module.cpp
            ${JERRYSCRIPT_DIR}/parser/js/js-parser-statm.cpp
            ${JERRYSCRIPT_DIR}/parser/js/js-parser-tagged-template-literal.cpp
            ${JERRYSCRIPT_DIR}/parser/js/js-parser-util.cpp
            ${JERRYSCRIPT_DIR}/parser/js/js-scanner.cpp
            ${JERRYSCRIPT_DIR}/parser/js/js-scanner-ops.cpp
            ${JERRYSCRIPT_DIR}/parser/js/js-scanner-util.cpp
            ${JERRYSCRIPT_DIR}/parser/js/parser-errors.cpp
            # parser/regexp
            ${JERRYSCRIPT_DIR}/parser/regexp/re-bytecode.cpp
            ${JERRYSCRIPT_DIR}/parser/regexp/re-compiler.cpp
            ${JERRYSCRIPT_DIR}/parser/regexp/re-parser.cpp
            # vm
            ${JERRYSCRIPT_DIR}/vm/opcodes.cpp
            ${JERRYSCRIPT_DIR}/vm/opcodes-ecma-arithmetics.cpp
            ${JERRYSCRIPT_DIR}/vm/opcodes-ecma-bitwise.cpp
            ${JERRYSCRIPT_DIR}/vm/opcodes-ecma-relational-equality.cpp
            ${JERRYSCRIPT_DIR}/vm/vm.cpp
            ${JERRYSCRIPT_DIR}/vm/vm-stack.cpp
            ${JERRYSCRIPT_DIR}/vm/vm-utils.cpp
            # ecma/base
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-alloc.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-errors.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-extended-info.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-gc.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-helpers.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-helpers-collection.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-helpers-conversion.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-helpers-errol.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-helpers-external-pointers.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-helpers-number.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-helpers-string.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-helpers-value.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-init-finalize.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-lcache.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-literal-storage.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-module.cpp
            ${JERRYSCRIPT_DIR}/ecma/base/ecma-property-hashmap.cpp
            # ecma/operations
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-arguments-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-array-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-arraybuffer-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-async-generator-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-atomics-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-big-uint.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-bigint.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-bigint-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-boolean-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-comparison.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-container-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-conversion.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-dataview-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-eval.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-exceptions.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-function-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-get-put-value.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-iterator-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-jobqueue.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-lex-env.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-number-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-objects.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-objects-general.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-promise-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-proxy-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-reference.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-regexp-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-shared-arraybuffer-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-string-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-symbol-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/operations/ecma-typedarray-object.cpp
            # ecma/builtin-objects
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtins.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-aggregateerror.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-aggregateerror-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-array.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-array-iterator-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-array-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-array-prototype-unscopables.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-arraybuffer.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-arraybuffer-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-async-from-sync-iterator-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-async-function.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-async-function-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-async-generator.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-async-generator-function.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-async-generator-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-async-iterator-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-atomics.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-bigint.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-bigint-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-boolean.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-boolean-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-dataview.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-dataview-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-date.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-date-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-error.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-error-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-evalerror.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-evalerror-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-function.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-function-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-generator.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-generator-function.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-generator-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-global.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-handlers.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-helpers.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-helpers-date.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-helpers-error.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-helpers-sort.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-intrinsic.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-iterator-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-map.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-map-iterator-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-map-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-math.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-number.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-number-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-object.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-object-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-promise.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-promise-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-proxy.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-rangeerror.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-rangeerror-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-referenceerror.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-referenceerror-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-reflect.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-regexp.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-regexp-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-regexp-string-iterator-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-set.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-set-iterator-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-set-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-shared-arraybuffer.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-shared-arraybuffer-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-string.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-string-iterator-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-string-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-symbol.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-symbol-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-syntaxerror.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-syntaxerror-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-type-error-thrower.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-typeerror.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-typeerror-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-urierror.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-urierror-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-weakmap.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-weakmap-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-weakref.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-weakref-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-weakset.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/ecma-builtin-weakset-prototype.cpp
            # ecma/builtin-objects/typedarray
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-typedarray.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-typedarray-helpers.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-typedarray-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-bigint64array.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-bigint64array-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-biguint64array.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-biguint64array-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-float32array.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-float32array-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-float64array.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-float64array-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-int8array.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-int8array-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-int16array.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-int16array-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-int32array.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-int32array-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-uint8array.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-uint8array-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-uint8clampedarray.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-uint8clampedarray-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-uint16array.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-uint16array-prototype.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-uint32array.cpp
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray/ecma-builtin-uint32array-prototype.cpp
        )
        
        list(APPEND THORVG_PRIVATE_INCLUDE_DIRS
            ${JERRYSCRIPT_DIR}/api
            ${JERRYSCRIPT_DIR}/include
            ${JERRYSCRIPT_DIR}/jcontext
            ${JERRYSCRIPT_DIR}/jmem
            ${JERRYSCRIPT_DIR}/jrt
            ${JERRYSCRIPT_DIR}/lit
            ${JERRYSCRIPT_DIR}/parser/js
            ${JERRYSCRIPT_DIR}/parser/regexp
            ${JERRYSCRIPT_DIR}/vm
            ${JERRYSCRIPT_DIR}/ecma/base
            ${JERRYSCRIPT_DIR}/ecma/operations
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects
            ${JERRYSCRIPT_DIR}/ecma/builtin-objects/typedarray
        )
    endif()
endif()

# PNG Loader (internal)
if(TVG_LOADER_PNG)
    if(NOT TVG_STATIC_MODULES)
        find_package(PNG QUIET)
    endif()
    
    if(PNG_FOUND AND NOT TVG_STATIC_MODULES)
        list(APPEND THORVG_SOURCES
            src/loaders/external_png/tvgPngLoader.cpp
        )
        list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/external_png)
    else()
        list(APPEND THORVG_SOURCES
            src/loaders/png/tvgLodePng.cpp
            src/loaders/png/tvgPngLoader.cpp
        )
        list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/png)
    endif()
else()
    # Always include PNG headers for conditional compilation
    list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/png)
endif()

# JPG Loader (internal)
if(TVG_LOADER_JPG)
    if(NOT TVG_STATIC_MODULES)
        find_package(JPEG QUIET)
    endif()
    
    if(JPEG_FOUND AND NOT TVG_STATIC_MODULES)
        list(APPEND THORVG_SOURCES
            src/loaders/external_jpg/tvgJpgLoader.cpp
        )
        list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/external_jpg)
    else()
        list(APPEND THORVG_SOURCES
            src/loaders/jpg/tvgJpgd.cpp
            src/loaders/jpg/tvgJpgLoader.cpp
        )
        list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/jpg)
    endif()
else()
    # Always include JPG headers for conditional compilation
    list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/jpg)
endif()

# WebP Loader (internal)
if(TVG_LOADER_WEBP)
    if(NOT TVG_STATIC_MODULES)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(WEBP libwebp QUIET)
        endif()
    endif()
    
    if(WEBP_FOUND AND NOT TVG_STATIC_MODULES)
        list(APPEND THORVG_SOURCES
            src/loaders/external_webp/tvgWebpLoader.cpp
        )
        list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/external_webp)
    else()
        list(APPEND THORVG_SOURCES
            # dec
            src/loaders/webp/dec/alpha.cpp
            src/loaders/webp/dec/buffer.cpp
            src/loaders/webp/dec/frame.cpp
            src/loaders/webp/dec/io.cpp
            src/loaders/webp/dec/quant.cpp
            src/loaders/webp/dec/tree.cpp
            src/loaders/webp/dec/vp8.cpp
            src/loaders/webp/dec/vp8l.cpp
            src/loaders/webp/dec/webp.cpp
            # dsp
            src/loaders/webp/dsp/alpha_processing.cpp
            src/loaders/webp/dsp/argb.cpp
            src/loaders/webp/dsp/cpu.cpp
            src/loaders/webp/dsp/dec.cpp
            src/loaders/webp/dsp/dec_clip_tables.cpp
            src/loaders/webp/dsp/filters.cpp
            src/loaders/webp/dsp/lossless.cpp
            src/loaders/webp/dsp/rescaler.cpp
            src/loaders/webp/dsp/upsampling.cpp
            src/loaders/webp/dsp/yuv.cpp
            # utils
            src/loaders/webp/utils/bit_reader.cpp
            src/loaders/webp/utils/color_cache.cpp
            src/loaders/webp/utils/huffman.cpp
            src/loaders/webp/utils/quant_levels_dec.cpp
            src/loaders/webp/utils/random.cpp
            src/loaders/webp/utils/rescaler.cpp
            # loader
            src/loaders/webp/tvgWebpLoader.cpp
        )
        list(APPEND THORVG_PRIVATE_INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/webp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/webp/dec
            ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/webp/dsp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/webp/utils
            ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/webp/webp
        )
    endif()
else()
    # Always include WebP headers for conditional compilation
    list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/webp)
endif()

# Raw Loader (always included)
list(APPEND THORVG_SOURCES
    src/loaders/raw/tvgRawLoader.cpp
)
list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/raw)

# Saver include directory
list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/savers)

# GIF Saver
if(TVG_SAVER_GIF OR TVG_TOOL_LOTTIE2GIF)
    list(APPEND THORVG_SOURCES
        src/savers/gif/tvgGifEncoder.cpp
        src/savers/gif/tvgGifSaver.cpp
    )
endif()
# Always include GIF saver headers for conditional compilation
list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/savers/gif)

# C API Bindings
if(TVG_BINDINGS_CAPI)
    list(APPEND THORVG_SOURCES
        src/bindings/capi/tvgCapi.cpp
    )
    list(APPEND THORVG_PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/capi)
endif()

# Create the library
add_library(thorvg ${TVG_LIB_TYPE} ${THORVG_SOURCES})

target_include_directories(thorvg
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include/thorvg-${THORVG_VERSION_MAJOR}>
    PRIVATE
        ${THORVG_PRIVATE_INCLUDE_DIRS}
)

target_compile_options(thorvg
    PUBLIC ${THORVG_COMPILE_FLAGS}
    PRIVATE ${THORVG_PRIVATE_COMPILE_FLAGS}
)

# Add definition for test directory
target_compile_definitions(thorvg PRIVATE
    TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/resources"
)

# Link libraries
set(THORVG_LINK_LIBS "")

# Threading support
if(TVG_THREADS)
    if(NOT WIN32 AND NOT ANDROID)
        find_package(Threads REQUIRED)
        list(APPEND THORVG_LINK_LIBS Threads::Threads)
    endif()
endif()

# OpenMP support
if(TVG_OPENMP)
    find_package(OpenMP QUIET)
    if(OpenMP_CXX_FOUND)
        list(APPEND THORVG_LINK_LIBS OpenMP::OpenMP_CXX)
    endif()
endif()

# External PNG
if(PNG_FOUND AND NOT TVG_STATIC_MODULES AND TVG_LOADER_PNG)
    list(APPEND THORVG_LINK_LIBS PNG::PNG)
endif()

# External JPEG
if(JPEG_FOUND AND NOT TVG_STATIC_MODULES AND TVG_LOADER_JPG)
    list(APPEND THORVG_LINK_LIBS JPEG::JPEG)
endif()

# External WebP
if(WEBP_FOUND AND NOT TVG_STATIC_MODULES AND TVG_LOADER_WEBP)
    list(APPEND THORVG_LINK_LIBS ${WEBP_LIBRARIES})
    target_include_directories(thorvg PRIVATE ${WEBP_INCLUDE_DIRS})
endif()

# WebGPU
if(TVG_ENGINE_WG AND WGPU_FOUND)
    list(APPEND THORVG_LINK_LIBS ${WGPU_LIBRARIES})
    target_include_directories(thorvg PRIVATE ${WGPU_INCLUDE_DIRS})
endif()

target_link_libraries(thorvg PRIVATE ${THORVG_LINK_LIBS})

# Set library properties
set_target_properties(thorvg PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${THORVG_VERSION_MAJOR}
    OUTPUT_NAME "thorvg-${THORVG_VERSION_MAJOR}"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

if(WIN32)
    set_target_properties(thorvg PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
    )
endif()

# Install targets
include(GNUInstallDirs)

install(TARGETS thorvg
    EXPORT thorvgTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES inc/thorvg.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/thorvg-${THORVG_VERSION_MAJOR}
)

if(TVG_BINDINGS_CAPI)
    install(FILES src/bindings/capi/thorvg_capi.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/thorvg-${THORVG_VERSION_MAJOR}
    )
endif()

if(TVG_LOADER_LOTTIE OR TVG_TOOL_LOTTIE2GIF)
    install(FILES src/loaders/lottie/thorvg_lottie.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/thorvg-${THORVG_VERSION_MAJOR}
    )
endif()

# pkg-config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/thorvg.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/thorvg-${THORVG_VERSION_MAJOR}.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/thorvg-${THORVG_VERSION_MAJOR}.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# CMake package config
install(EXPORT thorvgTargets
    FILE thorvgTargets.cmake
    NAMESPACE thorvg::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/thorvg
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/thorvgConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/thorvgConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/thorvg
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/thorvgConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/thorvgConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/thorvgConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/thorvg
)

# Tools
if(TVG_TOOL_SVG2PNG)
    add_executable(tvg-svg2png
        tools/svg2png/svg2png.cpp
        tools/svg2png/lodepng.cpp
    )
    target_include_directories(tvg-svg2png PRIVATE
        ${THORVG_INCLUDE_DIRS}
    )
    target_link_libraries(tvg-svg2png PRIVATE thorvg)
    target_compile_options(tvg-svg2png PRIVATE ${THORVG_COMPILE_FLAGS})
    
    if(NOT TVG_BUILD_SHARED)
        target_compile_definitions(tvg-svg2png PRIVATE TVG_STATIC)
    endif()
    
    install(TARGETS tvg-svg2png
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(TVG_TOOL_LOTTIE2GIF)
    add_executable(tvg-lottie2gif
        tools/lottie2gif/lottie2gif.cpp
    )
    target_include_directories(tvg-lottie2gif PRIVATE
        ${THORVG_INCLUDE_DIRS}
    )
    target_link_libraries(tvg-lottie2gif PRIVATE thorvg)
    target_compile_options(tvg-lottie2gif PRIVATE ${THORVG_COMPILE_FLAGS})
    
    if(NOT TVG_BUILD_SHARED)
        target_compile_definitions(tvg-lottie2gif PRIVATE TVG_STATIC)
    endif()
    
    install(TARGETS tvg-lottie2gif
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Tests
if(TVG_BUILD_TESTS)
    enable_testing()
    
    set(TEST_SOURCES
        test/testAccessor.cpp
        test/testAnimation.cpp
        test/testFill.cpp
        test/testInitializer.cpp
        test/testLottie.cpp
        test/testMain.cpp
        test/testPaint.cpp
        test/testPicture.cpp
        test/testSavers.cpp
        test/testScene.cpp
        test/testShape.cpp
        test/testSwCanvas.cpp
        test/testSwEngine.cpp
        test/testText.cpp
    )
    
    add_executable(tvgUnitTests ${TEST_SOURCES})
    target_include_directories(tvgUnitTests PRIVATE
        ${THORVG_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/test
    )
    target_link_libraries(tvgUnitTests PRIVATE thorvg)
    
    if(NOT TVG_BUILD_SHARED)
        target_compile_definitions(tvgUnitTests PRIVATE TVG_STATIC)
    endif()
    
    if(APPLE)
        find_library(COCOA_FRAMEWORK Cocoa)
        find_library(IOKIT_FRAMEWORK IOKit)
        target_link_libraries(tvgUnitTests PRIVATE ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK})
    endif()
    
    add_test(NAME UnitTests COMMAND tvgUnitTests --success)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "ThorVG ${PROJECT_VERSION} Configuration Summary")
message(STATUS "=======================================")
message(STATUS "Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "Library Type:      ${TVG_LIB_TYPE}")
message(STATUS "Install Prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  File I/O:        ${TVG_FILE_IO}")
message(STATUS "  Multi-Tasking:   ${TVG_THREADS}")
message(STATUS "  Partial Render:  ${TVG_PARTIAL}")
message(STATUS "  SIMD:            ${TVG_SIMD}")
message(STATUS "  Log Message:     ${TVG_LOG}")
message(STATUS "  Tests:           ${TVG_BUILD_TESTS}")
message(STATUS "")
message(STATUS "Raster Engines:")
message(STATUS "  SW:              ${TVG_ENGINE_SW}")
message(STATUS "  GL:              ${TVG_ENGINE_GL}")
message(STATUS "  WG:              ${TVG_ENGINE_WG}")
message(STATUS "")
message(STATUS "Loaders:")
message(STATUS "  SVG:             ${TVG_LOADER_SVG}")
message(STATUS "  TTF:             ${TVG_LOADER_TTF}")
message(STATUS "  LOTTIE:          ${TVG_LOADER_LOTTIE}")
message(STATUS "  PNG:             ${TVG_LOADER_PNG}")
message(STATUS "  JPG:             ${TVG_LOADER_JPG}")
message(STATUS "  WEBP:            ${TVG_LOADER_WEBP}")
message(STATUS "")
message(STATUS "Savers:")
message(STATUS "  GIF:             ${TVG_SAVER_GIF}")
message(STATUS "")
message(STATUS "Bindings:")
message(STATUS "  CAPI:            ${TVG_BINDINGS_CAPI}")
message(STATUS "")
message(STATUS "Tools:")
message(STATUS "  svg2png:         ${TVG_TOOL_SVG2PNG}")
message(STATUS "  lottie2gif:      ${TVG_TOOL_LOTTIE2GIF}")
message(STATUS "")
message(STATUS "Extra:")
message(STATUS "  Lottie Expr:     ${TVG_LOTTIE_EXPRESSIONS}")
message(STATUS "  OpenMP:          ${TVG_OPENMP}")
if(TVG_ENGINE_GL)
    if(TVG_OPENGL_ES)
        message(STATUS "  OpenGL Variant:  OpenGL ES")
    else()
        message(STATUS "  OpenGL Variant:  OpenGL")
    endif()
endif()
message(STATUS "")
