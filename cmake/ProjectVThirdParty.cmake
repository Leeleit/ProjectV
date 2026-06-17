function(_projectv_collect_buildsystem_targets out_var directory_path)
    get_property(local_targets DIRECTORY "${directory_path}" PROPERTY BUILDSYSTEM_TARGETS)
    set(all_targets ${local_targets})

    get_property(child_directories DIRECTORY "${directory_path}" PROPERTY SUBDIRECTORIES)
    foreach (child_directory IN LISTS child_directories)
        _projectv_collect_buildsystem_targets(child_targets "${child_directory}")
        list(APPEND all_targets ${child_targets})
    endforeach ()

    list(REMOVE_DUPLICATES all_targets)
    set(${out_var} "${all_targets}" PARENT_SCOPE)
endfunction()

function(projectv_suppress_external_warnings directory_path)
    _projectv_collect_buildsystem_targets(directory_targets "${directory_path}")
    foreach (target_name IN LISTS directory_targets)
        if (NOT TARGET "${target_name}")
            continue()
        endif ()

        get_target_property(target_type "${target_name}" TYPE)
        if (NOT target_type OR target_type STREQUAL "UTILITY")
            continue()
        endif ()

        # **SYSTEM property (2026-06-18, windows-host-build-r0).**
        # Per `AGENTS.md §7.2.7` no blanket `-Wno-X` suppressions
        # belong on the consuming target. The clean cross-
        # platform fix is to mark external headers as SYSTEM
        # include directories — CMake (3.25+) propagates that
        # via the `SYSTEM` target property, which makes the
        # generator emit `-isystem` for GCC/Clang (warnings
        # auto-suppressed) and `/external:I path` +
        # `/external:W0` for MSVC (warnings in external
        # headers auto-suppressed under Ninja + MSVC 16.10+).
        # Works for INTERFACE / STATIC / SHARED / OBJECT
        # / EXECUTABLE targets alike — the INTERFACE branch
        # (was previously skipped) is exactly what we need for
        # header-only libraries like `VulkanMemoryAllocator`
        # and `fastgltf`, where the warning surfaces when the
        # header is parsed inside a consumer TU rather than
        # when the library itself is compiled.
        set_target_properties("${target_name}" PROPERTIES SYSTEM TRUE)

        # **Legacy per-target warnings-as-errors gate
        # (2026-06-15, windows-build-verification).** The
        # `/W0` + `-w` blanket-suppress below is kept for
        # projects that still configure against the old
        # `PROJECTV_BUILD_*` / `MSVC` shape (e.g. when
        # consuming ProjectV from a sibling workspace that
        # has not yet picked up the CMake 3.25+ SYSTEM
        # property). New builds should rely on the
        # `SYSTEM` flag above and do not need this.
        # `target_compile_options` / `target_compile_definitions`
        # only accept INTERFACE scope on INTERFACE targets,
        # so we branch on `target_type` to keep the call shape
        # valid for header-only libraries too.
        if (target_type STREQUAL "INTERFACE_LIBRARY")
            set(_PROJECTV_PROP_SCOPE INTERFACE)
        else ()
            set(_PROJECTV_PROP_SCOPE PRIVATE)
        endif ()
        if (MSVC)
            target_compile_options("${target_name}" ${_PROJECTV_PROP_SCOPE} /W0)
            if (CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                target_compile_options(
                        "${target_name}"
                        ${_PROJECTV_PROP_SCOPE}
                        /clang:-Wno-everything
                        /clang:-Wno-unused-command-line-argument
                )
            endif ()
            target_compile_definitions(
                    "${target_name}"
                    ${_PROJECTV_PROP_SCOPE}
                    _CRT_SECURE_NO_WARNINGS
                    _CRT_NONSTDC_NO_WARNINGS
            )
        elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options("${target_name}" ${_PROJECTV_PROP_SCOPE} -w)
        endif ()
        unset(_PROJECTV_PROP_SCOPE)
    endforeach ()
endfunction()

function(projectv_cleanup_ninja_subbuild_state build_root)
    file(
            GLOB_RECURSE
            projectv_ninja_temp_logs
            LIST_DIRECTORIES FALSE
            "${build_root}/_deps/*-subbuild/.ninja_deps"
            "${build_root}/_deps/*-subbuild/.ninja_lock"
            "${build_root}/_deps/*-subbuild/.ninja_log"
            "${build_root}/_deps/*-subbuild/.ninja_*.recompact"
            "${build_root}/_deps/*-subbuild/.ninja_*.restat"
    )

    if (projectv_ninja_temp_logs)
        file(REMOVE ${projectv_ninja_temp_logs})
    endif ()
endfunction()

function(projectv_add_tracy)
    if (PROJECTV_BUILD_TRACY_PROFILER AND NOT PROJECTV_ENABLE_TRACY)
        message(FATAL_ERROR "PROJECTV_BUILD_TRACY_PROFILER requires PROJECTV_ENABLE_TRACY=ON")
    endif ()

    if (NOT PROJECTV_ENABLE_TRACY)
        return()
    endif ()

    set(TRACY_ENABLE ON CACHE BOOL "Enable profiling" FORCE)
    add_subdirectory("${PROJECT_SOURCE_DIR}/external/tracy" "${CMAKE_BINARY_DIR}/external/tracy" EXCLUDE_FROM_ALL)
    projectv_suppress_external_warnings("${PROJECT_SOURCE_DIR}/external/tracy")

    if (PROJECTV_BUILD_TRACY_PROFILER)
        projectv_cleanup_ninja_subbuild_state("${CMAKE_BINARY_DIR}")
        set(projectv_saved_policy_version_minimum "${CMAKE_POLICY_VERSION_MINIMUM}")
        set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
        add_subdirectory(
                "${PROJECT_SOURCE_DIR}/external/tracy/profiler"
                "${CMAKE_BINARY_DIR}/external/tracy/profiler"
                EXCLUDE_FROM_ALL
        )
        if (DEFINED projectv_saved_policy_version_minimum AND NOT projectv_saved_policy_version_minimum STREQUAL "")
            set(CMAKE_POLICY_VERSION_MINIMUM "${projectv_saved_policy_version_minimum}")
        else ()
            unset(CMAKE_POLICY_VERSION_MINIMUM)
        endif ()
        projectv_suppress_external_warnings("${PROJECT_SOURCE_DIR}/external/tracy/profiler")
    endif ()

    if (TARGET TracyClient AND MSVC)
        target_compile_options(TracyClient PRIVATE /W0)
    endif ()
endfunction()
