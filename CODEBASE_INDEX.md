# Codebase File and Function Index

**Repository:** `benimprojem/OMNI-CORE-v2-NEXUS-FLOW`  
**Branch:** `refactor/c-core-roadmap`  
**Snapshot commit:** `3c5c6a2379a22595d585b8839cc28fa40c80bebc`  
**Scope:** `omnicore-sdk/src/c/`  
**Snapshot inventory:** 126 C/header files (91 C implementation files and 35 headers).  
**Purpose:** prevent duplicate files/functions, make existing implementation discoverable before adding or changing code.

## Maintenance rule

Before creating a source file or function, search this index and the current branch. Inspect the actual implementation and its callers; this index is a navigation aid, not a substitute for source review. Reuse or extend an existing implementation where appropriate. After every source change, update the file/function list, responsibilities, and snapshot commit. Never treat a stale line number or stale function entry as authoritative.

## Inventory completeness

The file/path inventory below is complete for the snapshot. The function catalogue now covers all non-test production `.c` implementation files in this snapshot. The `tests/` directory is fully listed by filename, but per-test function names are pending. Header declarations are listed by filename and should be indexed separately as public API declarations. The first-pass extractor is mechanical: review macro-related or unusual declarations against source before relying on a symbol as a stable API. The OIR Verifier source now calls the existing `oir_opcode_validate_shape()` registry helper; this function is not reimplemented inside the verifier. Its first dependency-ordered pass is `validate_structure()`, which validates primitive type records, block ownership, instruction coverage and function entry ownership. The second pass, `validate_ssa_definitions()`, validates the value/definition cross-reference and builds definition lookup tables. The third pass, `validate_instructions()`, reuses the shared opcode-shape registry, validates implemented opcode/type constraints and collects CFG predecessor edges. The fourth pass, `validate_terminators()`, enforces one final terminator per block and rejects instructions after a terminator. The fifth and sixth passes, `validate_dominance()` and `validate_ssa_uses()`, compute per-function reachability/dominators and then validate same-block order plus cross-block SSA dominance.

## Verified function catalogue

### `omnicore-sdk/src/c/module/oir_verify_stage_module.c`

`get_type()`, `get_value()`, `get_operand()`, `get_value_operand()`, `get_block_operand()`, `type_is_numeric()`, `bit_get()`, `bit_set()`, `has_terminator()`, `validate_structure()`, `validate_ssa_definitions()`, `validate_instructions()`, `validate_terminators()`, `validate_dominance()`, `validate_ssa_uses()`, `validate_module()`, `find_sections()`, `report_verify_error()`, `oir_verify_process()`, `occ_get_stage_module_v1()`

### `omnicore-sdk/src/c/module/oir_lowering_stage_module.c`

`find_payload()`, `add_array_bytes()`, `report_lowering_error()`, `oir_lowering_process()`, `occ_get_stage_module_v1()`

### `omnicore-sdk/src/c/module/occ_module_loader.c`

`set_error()`, `close_library()`, `occ_module_loader_open()`, `occ_module_loader_close()`, `occ_module_loader_invoke()`

### `omnicore-sdk/src/c/oir/oir_module.c`

`g16()`, `g32()`, `g64()`, `p16()`, `p32()`, `p64()`, `utf8()`, `range()`, `oir_module_open()`, `oir_module_write()`

### `omnicore-sdk/src/c/oir/oir_lowering.c`

`rd16()`, `rd32()`, `rd64()`, `get_field()`, `get_token_id()`, `get_node()`, `get_node_ref()`, `get_string_ref()`, `find_annotation()`, `find_semantic_type()`, `oir_lower_semantic_ast_literal_return()`

### `omnicore-sdk/src/c/codegen/codegen.c`

`codegen_binary()`, `codegen_func_decl()`, `codegen_node()`, `codegen_run()`

### `omnicore-sdk/src/c/core/compilation.c`

`occ_compilation_init()`, `occ_compilation_destroy()`, `occ_compilation_begin()`, `occ_compilation_complete()`, `occ_compilation_fail()`, `occ_compilation_cancel()`, `occ_compilation_can_publish_output()`

### `omnicore-sdk/src/c/core/diagnostics.c`

`reserve_records()`, `reserve_text()`, `occ_diagnostics_init()`, `occ_diagnostics_destroy()`, `occ_diagnostics_add()`, `occ_diagnostics_limit_exceeded()`, `occ_diagnostics_count()`, `occ_diagnostics_count_severity()`, `occ_diagnostics_get()`, `occ_diagnostics_blocks_output()`

### `omnicore-sdk/src/c/parser/lexer.c`

`peek()`, `peek_next()`, `advance()`, `match()`, `make_token()`, `error_token()`, `skip_whitespace()`, `check_keyword()`, `identifier_type()`, `lexer_next_token()`

### `omnicore-sdk/src/c/parser/parser.c`

`msg_init()`, `msg_destroy()`, `parser_context_init()`, `parser_init_source_ex()`, `parser_init_ex()`, `parser_mark_oom()`, `parser_status_is_terminal()`, `parser_should_fail_allocation()`, `parser_next_token()`, `parser_make_token_span()`, `error_at()`, `advance()`, `consume()`, `match()`, `check()`, `parser_is_statement_start()`, `parser_is_conditional_action_start()`, `parser_looks_like_conditional_action()`, `parser_synchronize_statement()`, `declaration()`, `parser_peek_n()`, `peek_token()`, `peek_token_ahead()`, `parse_struct_body()`, `parser_run()`, `parser_get_status()`, `parser_test_fail_allocation_at()`, `parser_add_issue()`, `parser_attach_diagnostics()`, `parser_attach_token_stream()`, `parser_print_errors()`, `parser_destroy()`, `parser_save_ir()`

### `omnicore-sdk/src/c/parser/semantic.c`

`semantic_init()`, `semantic_attach_diagnostics()`, `semantic_attach_node_id_resolver()`, `semantic_finalize_result()`, `semantic_should_fail_allocation()`, `semantic_test_fail_allocation_at()`, `semantic_test_get_allocation_attempts()`, `semantic_set_result_error()`, `semantic_node_id()`, `levenshtein()`, `report_issue()`, `report_error()`, `is_primitive_type_for_result()`, `semantic_result_symbol_kind()`, `semantic_type_id()`, `semantic_define_named_type()`, `semantic_record_annotation()`, `destroy_slot()`, `enter_scope()`, `exit_scope()`, `are_types_compatible()`, `is_numeric_primitive_type()`, `is_arithmetic_binary_operator()`, `is_integer_primitive_type()`, `is_type_defined()`, `semantic_is_boolean_condition()`, `semantic_check()`, `semantic_print_errors()`, `semantic_destroy()`

### `omnicore-sdk/src/c/semantic/semantic_result.c`

`should_fail_allocation()`, `semantic_result_test_fail_allocation_at()`, `semantic_result_test_get_allocation_attempts()`, `reserve_array()`, `valid_utf8()`, `names_equal()`, `valid_type_kind()`, `valid_symbol_kind()`, `valid_relation_kind()`, `relation_owner_kind_matches()`, `scope_is_ancestor()`, `compare_u32()`, `semantic_result_init()`, `semantic_result_destroy()`, `semantic_result_add_scope()`, `semantic_result_intern_type()`, `semantic_result_add_symbol()`, `semantic_result_set_symbol_type()`, `semantic_result_find_visible_symbol()`, `semantic_result_set_annotation()`, `semantic_result_add_relation()`, `semantic_result_add_conversion()`, `semantic_result_finalize()`

### `omnicore-sdk/src/c/oir/oir.c`

`read_u16_le()`, `read_u32_le()`, `read_u64_le()`, `write_u16_le()`, `write_u32_le()`, `write_u64_le()`, `is_known_section()`, `is_valid_validity()`, `oir_artifact_open()`, `oir_artifact_get_section()`, `oir_artifact_write()`

### `omnicore-sdk/src/c/oir/oir_payload.c`

`read_u32_le()`, `read_u64_le()`, `write_u16_le()`, `write_u32_le()`, `write_u64_le()`, `is_valid_utf8_text()`, `oir_source_unit_open()`, `oir_source_unit_write()`

### `omnicore-sdk/src/c/oir/oir_module_opcode.c`

`oir_opcode_registry_count()`, `oir_opcode_validate_shape()`

### `omnicore-sdk/src/c/module/occ_module_api.c`

`valid_artifact_kind()`, `valid_descriptor_fields()`, `host_report_diagnostic()`, `occ_module_diagnostic_sink_init_v1()`, `occ_module_validate_descriptor_v1()`, `valid_diagnostic_sink()`, `occ_module_validate_invocation_v1()`

### `omnicore-sdk/src/c/module/codegen_stage_module.c`

`asm_emit()`, `valid_assembler_identifier()`, `get_value()`, `get_type()`, `value_operand()`, `immediate_operand()`, `float_immediate_operand()`, `block_operand()`, `verify_codegen_module()`, `value_offset()`, `emit_instruction()`, `emit_module_assembly()`, `report_codegen_error()`, `codegen_stage_process()`, `occ_get_stage_module_v1()`

### `omnicore-sdk/src/c/module/semantic_stage_module.c`

`semantic_stage_load_messages()`, `semantic_stage_messages_once()`, `semantic_stage_ensure_messages()`, `find_payload()`, `resolve_node()`, `add_size()`, `add_name()`, `required_size()`, `send_diagnostics()`, `semantic_process()`, `msg_get()`

### `omnicore-sdk/src/c/target/target_profile.c`

`set_error()`, `valid_id()`, `device_index()`, `memory_index()`, `duplicate_device_id()`, `duplicate_memory_id()`, `duplicate_link_id()`, `occ_target_profile_validate()`

### `omnicore-sdk/src/c/target/target_profile_parser.c`

`fail()`, `remove_comment()`, `read_string()`, `next_csv_token()`, `read_u32()`, `read_list()`, `parse_device_kind()`, `add_atomic_widths()`, `add_fp_formats()`, `add_cpu_flags()`, `occ_target_profile_load()`

### `omnicore-sdk/src/c/module/lexer_stage_module.c`

`find_payload()`, `report_diagnostics()`, `lexer_stage_process()`, `occ_get_stage_module_v1()`

### `omnicore-sdk/src/c/module/parser_stage_module.c`

`find_payload()`, `report_parser_diagnostics()`, `parser_stage_process()`, `occ_get_stage_module_v1()`

### `omnicore-sdk/src/c/oir/oir_semantic_ast.c`

`read_u32_le()`, `write_u32_le()`, `validate_scope_records()`, `oir_semantic_scopes_open()`, `oir_semantic_scopes_get()`, `oir_semantic_scopes_write()`, `is_valid_utf8_name()`, `validate_type_values()`, `validate_type_records()`, `oir_semantic_types_open()`, `oir_semantic_types_get()`, `oir_semantic_types_write()`, `validate_symbol_values()`, `validate_symbol_records()`, `oir_semantic_symbols_open()`, `oir_semantic_symbols_get()`, `oir_semantic_symbols_write()`, `validate_annotation_records()`, `oir_semantic_annotations_open()`, `oir_semantic_annotations_get()`, `oir_semantic_annotations_write()`, `validate_relation_records()`, `oir_semantic_relations_open()`, `oir_semantic_relations_get()`, `oir_semantic_relations_write()`, `validate_conversion_records()`, `oir_semantic_conversions_open()`, `oir_semantic_conversions_get()`, `oir_semantic_conversions_write()`, `semantic_ast_node_kind()`, `semantic_symbol_declaration_kind_matches()`, `semantic_relation_node_kind_matches()`, `semantic_relation_owner_kind_matches()`, `semantic_scope_is_ancestor()`, `oir_semantic_ast_validate_references()`

### `omnicore-sdk/src/c/oir/oir_ast_artifact.c`

`oir_ast_artifact_write_frontend_ast()`, `oir_ast_artifact_read_frontend_ast()`

### `omnicore-sdk/src/c/oir/oir_diagnostics.c`

`read_u16_le()`, `read_u32_le()`, `read_u64_le()`, `write_u16_le()`, `write_u32_le()`, `write_u64_le()`, `is_valid_utf8_text()`, `valid_string()`, `read_record()`, `oir_diagnostics_payload_open()`, `oir_diagnostics_payload_get()`, `oir_diagnostics_payload_write()`

### `omnicore-sdk/src/c/codegen/target_parser.c`

`set_error()`, `strip_comment()`, `parse_string()`, `parse_unsigned()`, `parse_boolean()`, `validate_array()`, `known_section()`, `known_key()`, `parse_abi()`, `parse_simd()`, `parse_spec_version()`, `target_config_load()`, `strcmp()`, `strcmp()`, `strcmp()`, `target_backend_check()`

### `omnicore-sdk/src/c/core/failure_store.c`

`path_format()`, `ensure_directory()`, `occ_failure_store_save()`

### `omnicore-sdk/src/c/main.c`

`occ_print_job_diagnostics()`, `occ_validate_module_artifact()`, `occ_write_artifact_file()`, `occ_validate_assembly_artifact()`, `occ_run_codegen_stage()`, `occ_build_and_run_parser_module()`, `get_executable_dir()`, `path_join()`, `get_config_language()`, `list_targets()`, `occ_make_job_id()`, `occ_add_host_diagnostic()`, `occ_save_failure_checkpoint()`, `main()`

### `omnicore-sdk/src/c/oir/oir_ast_field_schema.c`

`required_field_count()`, `expected_type()`, `is_nullable_string()`, `oir_ast_field_schema_validate()`

### `omnicore-sdk/src/c/oir/oir_ast_frontend_index.c`

`pointer_hash()`, `insert_node()`, `oir_ast_frontend_index_lookup()`, `oir_ast_frontend_child_field_count()`, `visit_node()`, `oir_ast_frontend_index_build()`
### `omnicore-sdk/src/c/oir/oir_ast_frontend_reader.c`

`get_u16()`, `get_u32()`, `get_u64()`, `get_i32()`, `get_i64()`, `get_f64()`, `allocate_string()`, `free_disconnected_node()`, `cleanup_disconnected()`, `validate_scalar_ranges()`, `populate_scalar_fields()`, `set_child_pointer()`, `oir_ast_frontend_read_payload()`

### `omnicore-sdk/src/c/oir/oir_ast_frontend_writer.c`

`put_u16()`, `put_u32()`, `put_u64()`, `valid_utf8()`, `bounded_string_length()`, `append_tlv()`, `emit_null()`, `emit_bool()`, `emit_u32()`, `emit_i32()`, `emit_i64()`, `emit_f64()`, `emit_token()`, `emit_node_ref()`, `emit_string()`, `emit_node_fields()`, `emit_tree()`, `run_measure()`, `oir_ast_frontend_measure()`, `oir_ast_frontend_write_payload()`

### `omnicore-sdk/src/c/oir/oir_ast_graph.c`

`read_u32_le()`, `add_incoming()`, `oir_ast_graph_validate()`

### `omnicore-sdk/src/c/oir/oir_ast_map.c`

`oir_ast_node_id_from_frontend()`, `oir_ast_node_id_to_frontend()`

### `omnicore-sdk/src/c/oir/oir_ast_node_table.c`

`read_u16_le()`, `read_u32_le()`, `write_u16_le()`, `write_u32_le()`, `valid_utf8()`, `validate_node_fields()`, `validate_host_records()`, `oir_ast_node_table_open()`, `oir_ast_node_table_get()`, `oir_ast_node_table_get_string()`, `oir_ast_node_table_get_field()`, `oir_ast_node_table_write()`
### `omnicore-sdk/src/c/oir/oir_ast_tlv.c`

`read_u16_le()`, `read_u32_le()`, `write_u16_le()`, `write_u32_le()`, `value_length_is_valid()`, `oir_ast_tlv_writer_init()`, `oir_ast_tlv_write()`, `oir_ast_tlv_reader_init()`, `oir_ast_tlv_reader_next()`, `oir_ast_tlv_validate()`

### `omnicore-sdk/src/c/oir/oir_failure_artifact.c`

`oir_failure_artifact_write()`

### `omnicore-sdk/src/c/oir/oir_lowering_expr.c`

`u16()`, `u32()`, `u64()`, `node()`, `field()`, `ref()`, `token()`, `string_ref()`, `annotation_get()`, `annotation_type()`, `check_type()`, `ensure_bool_type()`, `begin_block()`, `finish_block()`, `alloc_inst()`, `lower_identifier()`, `symbol_matches_declaration()`, `lower_assignment_statement()`, `lower_literal()`, `lower_binary()`, `lower_expr()`, `lower_return_current_block()`, `lower_if_tree()`, `lower_branch()`, `oir_lower_semantic_ast_return()`

### `omnicore-sdk/src/c/oir/oir_semantic_ast_artifact.c`

`oir_semantic_ast_artifact_open()`

### `omnicore-sdk/src/c/oir/oir_semantic_ast_artifact_writer.c`

`writer_should_fail()`, `add_size()`, `count_string_bytes()`, `append_string()`, `build_type_strings()`, `build_symbol_strings()`, `build_relation_strings()`, `semantic_ast_artifact_write_impl()`, `oir_semantic_ast_artifact_write()`, `oir_semantic_ast_artifact_write_test_fail_at()`
### `omnicore-sdk/src/c/oir/oir_token_map.c`

`oir_token_id_from_omni()`, `oir_token_id_to_omni()`

### `omnicore-sdk/src/c/oir/oir_token_stream.c`

`read_u16_le()`, `read_u32_le()`, `read_u64_le()`, `write_u16_le()`, `write_u32_le()`, `write_u64_le()`, `validate_position()`, `validate_record()`, `decode_record()`, `oir_token_stream_input_from_lexer_token()`, `oir_token_stream_record_to_lexer_token()`, `oir_token_stream_open()`, `oir_token_stream_get()`, `oir_token_stream_write()`

### `omnicore-sdk/src/c/oir/oir_type_map.c`

`oir_type_map_primitive()`

### `omnicore-sdk/src/c/parser/ast.c`

`get_node_size()`, `print_indent()`, `ast_print_recursive()`, `ast_print()`, `ast_destroy_recursive()`, `ast_destroy()`, `ast_write_exact()`, `write_string()`, `ast_read_exact()`, `ast_serialize_recursive()`, `ast_serialize()`
## Complete C/header file inventory

### `omnicore-sdk/src/c/codegen/`

- `codegen.c` — function catalogue reviewed
- `codegen.h` — function catalogue pending
- `target_parser.c` — function catalogue pending
- `target_parser.h` — function catalogue pending

### `omnicore-sdk/src/c/core/`

- `compilation.c` — function catalogue reviewed
- `compilation.h` — function catalogue pending
- `diagnostics.c` — function catalogue reviewed
- `diagnostics.h` — function catalogue pending
- `failure_store.c` — function catalogue pending
- `failure_store.h` — function catalogue pending

### `omnicore-sdk/src/c/`

- `main.c` — function catalogue pending

### `omnicore-sdk/src/c/module/`

- `codegen_stage_module.c` — function catalogue reviewed
- `lexer_stage_module.c` — function catalogue reviewed
- `occ_module_api.c` — function catalogue reviewed
- `occ_module_api.h` — function catalogue pending
- `occ_module_loader.c` — function catalogue reviewed
- `occ_module_loader.h` — function catalogue pending
- `oir_lowering_stage_module.c` — function catalogue reviewed
- `oir_verify_stage_module.c` — function catalogue reviewed
- `parser_stage_module.c` — function catalogue reviewed
- `semantic_stage_module.c` — function catalogue reviewed

### `omnicore-sdk/src/c/oir/`

- `oir.c` — function catalogue reviewed
- `oir.h` — function catalogue pending
- `oir_ast_artifact.c` — function catalogue reviewed
- `oir_ast_artifact.h` — function catalogue pending
- `oir_ast_field_schema.c` — function catalogue pending
- `oir_ast_field_schema.h` — function catalogue pending
- `oir_ast_frontend_index.c` — function catalogue pending
- `oir_ast_frontend_index.h` — function catalogue pending
- `oir_ast_frontend_reader.c` — function catalogue pending
- `oir_ast_frontend_reader.h` — function catalogue pending
- `oir_ast_frontend_writer.c` — function catalogue pending
- `oir_ast_frontend_writer.h` — function catalogue pending
- `oir_ast_graph.c` — function catalogue pending
- `oir_ast_graph.h` — function catalogue pending
- `oir_ast_map.c` — function catalogue pending
- `oir_ast_map.h` — function catalogue pending
- `oir_ast_node_table.c` — function catalogue pending
- `oir_ast_node_table.h` — function catalogue pending
- `oir_ast_tlv.c` — function catalogue pending
- `oir_ast_tlv.h` — function catalogue pending
- `oir_diagnostics.c` — function catalogue reviewed
- `oir_diagnostics.h` — function catalogue pending
- `oir_failure_artifact.c` — function catalogue pending
- `oir_failure_artifact.h` — function catalogue pending
- `oir_lowering.c` — function catalogue reviewed
- `oir_lowering.h` — function catalogue pending
- `oir_lowering_expr.c` — function catalogue pending
- `oir_module.c` — function catalogue reviewed
- `oir_module.h` — function catalogue pending
- `oir_module_opcode.c` — function catalogue reviewed
- `oir_module_opcode.h` — function catalogue pending
- `oir_payload.c` — function catalogue reviewed
- `oir_payload.h` — function catalogue pending
- `oir_semantic_ast.c` — function catalogue reviewed
- `oir_semantic_ast.h` — function catalogue pending
- `oir_semantic_ast_artifact.c` — function catalogue pending
- `oir_semantic_ast_artifact.h` — function catalogue pending
- `oir_semantic_ast_artifact_writer.c` — function catalogue pending
- `oir_token_map.c` — function catalogue pending
- `oir_token_map.h` — function catalogue pending
- `oir_token_stream.c` — function catalogue pending
- `oir_token_stream.h` — function catalogue pending
- `oir_type_map.c` — function catalogue pending
- `oir_type_map.h` — function catalogue pending

### `omnicore-sdk/src/c/parser/`

- `ast.c` — function catalogue pending
- `ast.h` — function catalogue pending
- `lexer.c` — function catalogue reviewed
- `lexer.h` — function catalogue pending
- `parser.c` — function catalogue reviewed
- `parser.h` — function catalogue pending
- `semantic.c` — function catalogue reviewed
- `semantic.h` — function catalogue pending

### `omnicore-sdk/src/c/semantic/`

- `semantic_result.c` — function catalogue reviewed
- `semantic_result.h` — function catalogue pending

### `omnicore-sdk/src/c/target/`

- `target_profile.c` — function catalogue reviewed
- `target_profile.h` — function catalogue pending
- `target_profile_parser.c` — function catalogue reviewed
- `target_profile_parser.h` — function catalogue pending

### `omnicore-sdk/src/c/tests/`

- `sample_stage_module.c` — function catalogue pending
- `test_ast_deserialize.c` — function catalogue pending
- `test_codegen_stage_module.c` — function catalogue pending
- `test_compilation.c` — function catalogue pending
- `test_diagnostics.c` — function catalogue pending
- `test_dynamic_stage_module.c` — function catalogue pending
- `test_failure_store.c` — function catalogue pending
- `test_lexer.c` — function catalogue pending
- `test_lexer_stage_module.c` — function catalogue pending
- `test_module_abi.c` — function catalogue pending
- `test_occ_module_api.c` — function catalogue pending
- `test_occ_module_loader.c` — function catalogue pending
- `test_oir.c` — function catalogue pending
- `test_oir_ast_artifact.c` — function catalogue pending
- `test_oir_ast_field_schema.c` — function catalogue pending
- `test_oir_ast_frontend_index.c` — function catalogue pending
- `test_oir_ast_frontend_reader.c` — function catalogue pending
- `test_oir_ast_frontend_writer.c` — function catalogue pending
- `test_oir_ast_graph.c` — function catalogue pending
- `test_oir_ast_map.c` — function catalogue pending
- `test_oir_ast_node_table.c` — function catalogue pending
- `test_oir_ast_tlv.c` — function catalogue pending
- `test_oir_diagnostics.c` — function catalogue pending
- `test_oir_failure_artifact.c` — function catalogue pending
- `test_oir_lowering.c` — function catalogue pending
- `test_oir_module.c` — function catalogue pending
- `test_oir_semantic_ast.c` — function catalogue pending
- `test_oir_semantic_ast_artifact.c` — function catalogue pending
- `test_oir_semantic_ast_refs.c` — function catalogue pending
- `test_oir_source_unit.c` — function catalogue pending
- `test_oir_token_map.c` — function catalogue pending
- `test_oir_token_stream.c` — function catalogue pending
- `test_oir_type_map.c` — function catalogue pending
- `test_oir_verify_stage_module.c` — function catalogue pending
- `test_parser.c` — function catalogue pending
- `test_parser_ast_oir_roundtrip.c` — function catalogue pending
- `test_parser_oir_handoff.c` — function catalogue pending
- `test_parser_stage_module.c` — function catalogue pending
- `test_semantic.c` — function catalogue pending
- `test_semantic_arithmetic.c` — function catalogue pending
- `test_semantic_oom.c` — function catalogue pending
- `test_semantic_result.c` — function catalogue pending
- `test_semantic_result_integration.c` — function catalogue pending
- `test_semantic_result_oom.c` — function catalogue pending
- `test_semantic_stage_module.c` — function catalogue pending
- `test_target_parser.c` — function catalogue pending
- `test_target_profile.c` — function catalogue pending

## Module map and reuse hints

| Area | Existing responsibility / reuse target |
|---|---|
| `core/` | Compilation lifecycle, diagnostics and failure storage |
| `module/` | Shared stage ABI, loader and stage adapters for Lexer, Parser, Semantic, OIR Lowering, OIR Verifier and Codegen |
| `oir/` | OIR artifact/container, payloads, semantic AST records, maps, lowering, opcode registry and diagnostics |
| `parser/` | Lexer, parser/AST and semantic analysis |
| `semantic/` | Semantic result tables, symbols, types, scopes, annotations, relations and conversions |
| `target/` | Target profile model, validation and parser |
| `codegen/` | Existing code generation implementation and target parser |
| `tests/` | Unit, integration, ABI, stage-module, OIR and target-profile tests |

## Verifier-specific reuse map

- `module/oir_verify_stage_module.c`: `OirVerifyFailure` carries stable verifier diagnostic code plus available function/block/instruction/value/type IDs; `validate_module()` coordinates six validation passes; `report_verify_error()` maps codes to stable messages without changing the shared OCC ABI. Refactor this implementation; do not create a second verifier module.
- `oir/oir_module.c` + `oir/oir_module.h`: canonical OIR_MODULE reader/writer and accessors. Reuse its accessors rather than reimplementing payload decoding.
- `oir/oir_module_opcode.c` + `oir/oir_module_opcode.h`: opcode registry and `oir_opcode_validate_shape()` for operand count/kinds and result-presence rules. The verifier reuses this helper before opcode-specific semantic/type checks; extend the registry/contract rather than duplicating opcode tables.
- `module/occ_module_api.h` + `module/occ_module_api.c`: common stage ABI and diagnostics; do not add a verifier-only ABI.
- `tests/test_oir_verify_stage_module.c`: existing verifier fixtures and tests, including exact diagnostic-code assertions for SSA use, CFG edge, function entry, SSA definition, terminator and malformed payload; extend these before introducing a parallel test harness.
- `TASKS.md` and `OIR_VERIFIER_SPEC.md`: implementation backlog and behavioral contract.

## Pending indexing

- Function-level inventory for the files under `omnicore-sdk/src/c/tests/`.
- Public function declarations and types exported by all `.h` files.
- Non-C sources outside `omnicore-sdk/src/c/` (for example, build scripts and other language components) if they become relevant to an implementation task.

## Change log

- 2026-11-02: Initial source-tree snapshot and first-pass function catalogue added. File list covers all 126 C/header files under `omnicore-sdk/src/c/`; function-level extraction covers all 25 non-test production C implementation files. Test implementation function names and header API declarations remain pending.
- 2026-11-02: Verifier now reuses the existing opcode shape validator; a malformed extra-operand regression case was added. No new source function or file was introduced.