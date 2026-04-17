# Legacy docs structural merge report (2026-04-08 01:58:45)

Mode: heading-aware semantic merge for duplicate files.
Base scope: legacy/docs/old
Supplement source: HEAD:legacy/docs/old 2

Merged conflict files: 65

### Strategy
- Parse each file as markdown heading tree by #...######.
- Merge sections by heading path (level + normalized title).
- Deduplicate body lines inside each merged section, preserving order.
- Keep existing base order; append new sections from old2 at same parent level.

### Processed files
- legacy\docs\old\architecture\academic\01_project_defense_model.md -> 01_project_defense_model__old2.md
- legacy\docs\old\architecture\academic\02_mvp_defense_demo.md -> 02_mvp_defense_demo__old2.md
- legacy\docs\old\architecture\adr\0001-vulkan-renderer.md -> 0001-vulkan-renderer__old2.md
- legacy\docs\old\architecture\adr\0002-svo-storage.md -> 0002-svo-storage__old2.md
- legacy\docs\old\architecture\adr\0003-ecs-architecture.md -> 0003-ecs-architecture__old2.md
- legacy\docs\old\architecture\adr\0004-build-and-modules-spec.md -> 0004-build-and-modules-spec__old2.md
- legacy\docs\old\architecture\practice\01_core\01_engine_structure.md -> 01_engine_structure__old2.md
- legacy\docs\old\architecture\practice\01_core\02_core_loop.md -> 02_core_loop__old2.md
- legacy\docs\old\architecture\practice\01_core\03_engine_bootstrap.md -> 03_engine_bootstrap__old2.md
- legacy\docs\old\architecture\practice\01_core\04_custom_allocators.md -> 04_custom_allocators__old2.md
- legacy\docs\old\architecture\practice\01_core\05_job_system.md -> 05_job_system__old2.md
- legacy\docs\old\architecture\practice\01_core\06_zero_copy_memory.md -> 06_zero_copy_memory__old2.md
- legacy\docs\old\architecture\practice\01_core\07_error_handling.md -> 07_error_handling__old2.md
- legacy\docs\old\architecture\practice\01_core\08_shutdown_sequence.md -> 08_shutdown_sequence__old2.md
- legacy\docs\old\architecture\practice\01_core\09_cpp26_reality.md -> 09_cpp26_reality__old2.md
- legacy\docs\old\architecture\practice\02_render\01_vulkan_spec.md -> 01_vulkan_spec__old2.md
- legacy\docs\old\architecture\practice\02_render\02_gpu_staging.md -> 02_gpu_staging__old2.md
- legacy\docs\old\architecture\practice\02_render\03_gpu_debugging.md -> 03_gpu_debugging__old2.md
- legacy\docs\old\architecture\practice\02_render\04_render_graph.md -> 04_render_graph__old2.md
- legacy\docs\old\architecture\practice\03_voxel\01_svo_architecture.md -> 01_svo_architecture__old2.md
- legacy\docs\old\architecture\practice\03_voxel\02_voxel_pipeline.md -> 02_voxel_pipeline__old2.md
- legacy\docs\old\architecture\practice\03_voxel\03_voxel_sync_pipeline.md -> 03_voxel_sync_pipeline__old2.md
- legacy\docs\old\architecture\practice\03_voxel\04_svo_ca_bridge.md -> 04_svo_ca_bridge__old2.md
- legacy\docs\old\architecture\practice\04_physics_ca\01_jolt_vulkan_bridge.md -> 01_jolt_vulkan_bridge__old2.md
- legacy\docs\old\architecture\practice\04_physics_ca\02_physics_voxel_integration.md -> 02_physics_voxel_integration__old2.md
- legacy\docs\old\architecture\practice\04_physics_ca\03_gpu_cellular_automata.md -> 03_gpu_cellular_automata__old2.md
- legacy\docs\old\architecture\practice\04_physics_ca\04_cpu_gpu_physics_sync.md -> 04_cpu_gpu_physics_sync__old2.md
- legacy\docs\old\architecture\practice\04_physics_ca\07_dynamic_voxel_entities.md -> 07_dynamic_voxel_entities__old2.md
- legacy\docs\old\architecture\practice\05_ecs_gameplay\01_flecs_vulkan_bridge.md -> 01_flecs_vulkan_bridge__old2.md
- legacy\docs\old\architecture\practice\05_ecs_gameplay\02_coordinate_systems.md -> 02_coordinate_systems__old2.md
- legacy\docs\old\architecture\practice\05_ecs_gameplay\03_camera_controller.md -> 03_camera_controller__old2.md
- legacy\docs\old\architecture\practice\05_ecs_gameplay\04_input_system.md -> 04_input_system__old2.md
- legacy\docs\old\architecture\practice\05_ecs_gameplay\05_input_actions.md -> 05_input_actions__old2.md
- legacy\docs\old\architecture\practice\05_ecs_gameplay\06_game_ui.md -> 06_game_ui__old2.md
- legacy\docs\old\architecture\practice\06_assets\01_resource_management.md -> 01_resource_management__old2.md
- legacy\docs\old\architecture\practice\06_assets\02_vox_import.md -> 02_vox_import__old2.md
- legacy\docs\old\architecture\practice\06_assets\03_serialization.md -> 03_serialization__old2.md
- legacy\docs\old\architecture\practice\06_assets\04_reflection.md -> 04_reflection__old2.md
- legacy\docs\old\architecture\practice\06_assets\05_hot_reload.md -> 05_hot_reload__old2.md
- legacy\docs\old\architecture\practice\06_assets\06_asset_management.md -> 06_asset_management__old2.md
- legacy\docs\old\architecture\practice\06_assets\07_material_system.md -> 07_material_system__old2.md
- legacy\docs\old\architecture\practice\07_meta\01_team_workflow.md -> 01_team_workflow__old2.md
- legacy\docs\old\architecture\README.md -> README__old2.md
- legacy\docs\old\architecture\theory\01_ecs-concepts.md -> 01_ecs-concepts__old2.md
- legacy\docs\old\architecture\theory\02_memory-layout.md -> 02_memory-layout__old2.md
- legacy\docs\old\architecture\theory\03_system-communication.md -> 03_system-communication__old2.md
- legacy\docs\old\architecture\theory\04_caching.md -> 04_caching__old2.md
- legacy\docs\old\architecture\theory\05_memory-arenas.md -> 05_memory-arenas__old2.md
- legacy\docs\old\architecture\theory\06_composition.md -> 06_composition__old2.md
- legacy\docs\old\libraries\draco\02_integration.md -> 02_integration__old2.md
- legacy\docs\old\libraries\fastgltf\02_integration.md -> 02_integration__old2.md
- legacy\docs\old\standards\cmake\00_specification.md -> 00_specification__old2.md
- legacy\docs\old\standards\cmake\01_basics-structure.md -> 01_basics-structure__old2.md
- legacy\docs\old\standards\cmake\02_dependencies.md -> 02_dependencies__old2.md
- legacy\docs\old\standards\cmake\03_build-configuration.md -> 03_build-configuration__old2.md
- legacy\docs\old\standards\cmake\04_advanced-optimization.md -> 04_advanced-optimization__old2.md
- legacy\docs\old\standards\cmake\05_cross-platform.md -> 05_cross-platform__old2.md
- legacy\docs\old\standards\cmake\06_troubleshooting-ide.md -> 06_troubleshooting-ide__old2.md
- legacy\docs\old\standards\cpp\00_language-standard.md -> 00_language-standard__old2.md
- legacy\docs\old\standards\cpp\01_code-structure.md -> 01_code-structure__old2.md
- legacy\docs\old\standards\cpp\02_memory-management.md -> 02_memory-management__old2.md
- legacy\docs\old\standards\cpp\03_pimpl_dod_boundaries.md -> 03_pimpl_dod_boundaries__old2.md
- legacy\docs\old\standards\git\00_version-control-standard.md -> 00_version-control-standard__old2.md
- legacy\docs\old\standards\git\01_branching-strategy.md -> 01_branching-strategy__old2.md
- legacy\docs\old\standards\git\02_pull-request-process.md -> 02_pull-request-process__old2.md
