#include "target_profile_parser.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TARGET_V2_LINE_SIZE 512u
#define TARGET_V2_SEEN_MAX 64u
#define TARGET_V2_SEEN_SIZE 96u

static void fail(char* out, size_t size, const char* format, ...) {
    va_list args;
    if (!out || size == 0u) return;
    va_start(args, format);
    (void)vsnprintf(out, size, format, args);
    va_end(args);
}

static char* trim_ws(char* s) {
    char* end;
    while (*s && isspace((unsigned char)*s)) ++s;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

static void remove_comment(char* s) {
    int quoted = 0;
    char* p;
    for (p = s; *p; ++p) {
        if (*p == '"') quoted = !quoted;
        else if (*p == '#' && !quoted) { *p = '\0'; break; }
    }
}

static int read_string(const char* src, char* dst, size_t cap) {
    size_t n;
    const char* end;
    if (!src || !dst || cap == 0u || src[0] != '"') return 0;
    end = strrchr(src + 1, '"');
    if (!end) return 0;
    if (*trim_ws((char*)end + 1) != '\0') return 0;
    n = (size_t)(end - src - 1);
    if (n == 0u || n >= cap) return 0;
    memcpy(dst, src + 1, n);
    dst[n] = '\0';
    return 1;
}

static int read_u32(const char* src, uint32_t* out) {
    unsigned long value;
    char* end;
    if (!src || !*src || *src == '-') return 0;
    errno = 0;
    value = strtoul(src, &end, 10);
    while (*end && isspace((unsigned char)*end)) ++end;
    if (errno || end == src || *end || value > UINT32_MAX) return 0;
    *out = (uint32_t)value;
    return 1;
}

static int read_list(const char* src, char items[][OCC_TARGET_ISA_EXTENSION_SIZE],
                     uint32_t* count, uint32_t max_items) {
    char buffer[TARGET_V2_LINE_SIZE];
    char* token;
    if (!src || strlen(src) >= sizeof(buffer)) return 0;
    strcpy(buffer, src);
    *count = 0u;
    token = strtok(buffer, ",");
    while (token) {
        char* item = trim_ws(token);
        size_t n = strlen(item);
        if (n >= OCC_TARGET_ISA_EXTENSION_SIZE || n == 0u || *count >= max_items) return 0;
        strcpy(items[*count], item);
        ++*count;
        token = strtok(NULL, ",");
    }
    return 1;
}

static int parse_device_kind(const char* s, OccTargetDeviceKind* kind) {
    if (!strcmp(s,"cpu")) *kind=OCC_TARGET_DEVICE_CPU;
    else if (!strcmp(s,"gpu")) *kind=OCC_TARGET_DEVICE_GPU;
    else if (!strcmp(s,"npu")) *kind=OCC_TARGET_DEVICE_NPU;
    else if (!strcmp(s,"fapu")) *kind=OCC_TARGET_DEVICE_FAPU;
    else if (!strcmp(s,"dsp")) *kind=OCC_TARGET_DEVICE_DSP;
    else if (!strcmp(s,"fpga")) *kind=OCC_TARGET_DEVICE_FPGA;
    else return 0;
    return 1;
}

static int add_atomic_widths(const char* s, uint32_t* mask) {
    char copy[TARGET_V2_LINE_SIZE], *p;
    if (strlen(s) >= sizeof(copy)) return 0;
    strcpy(copy,s); *mask=0u;
    for (p=strtok(copy,","); p; p=strtok(NULL,",")) {
        p=trim_ws(p);
        if (!strcmp(p,"8")) *mask|=OCC_TARGET_ATOMIC_WIDTH_8;
        else if (!strcmp(p,"16")) *mask|=OCC_TARGET_ATOMIC_WIDTH_16;
        else if (!strcmp(p,"32")) *mask|=OCC_TARGET_ATOMIC_WIDTH_32;
        else if (!strcmp(p,"64")) *mask|=OCC_TARGET_ATOMIC_WIDTH_64;
        else if (!strcmp(p,"128")) *mask|=OCC_TARGET_ATOMIC_WIDTH_128;
        else return 0;
    }
    return 1;
}

static int add_fp_formats(const char* s, uint32_t* mask) {
    char copy[TARGET_V2_LINE_SIZE], *p;
    if (strlen(s) >= sizeof(copy)) return 0;
    strcpy(copy,s); *mask=0u;
    for (p=strtok(copy,","); p; p=strtok(NULL,",")) {
        p=trim_ws(p);
        if (!strcmp(p,"fp16")) *mask|=OCC_TARGET_FP_FORMAT_FP16;
        else if (!strcmp(p,"bf16")) *mask|=OCC_TARGET_FP_FORMAT_BF16;
        else if (!strcmp(p,"fp32")) *mask|=OCC_TARGET_FP_FORMAT_FP32;
        else if (!strcmp(p,"fp64")) *mask|=OCC_TARGET_FP_FORMAT_FP64;
        else if (!strcmp(p,"fp128")) *mask|=OCC_TARGET_FP_FORMAT_FP128;
        else return 0;
    }
    return 1;
}

static int add_cpu_flags(const char* s, uint32_t* flags) {
    char copy[TARGET_V2_LINE_SIZE], *p;
    if (strlen(s) >= sizeof(copy)) return 0;
    strcpy(copy,s); *flags=0u;
    for (p=strtok(copy,","); p; p=strtok(NULL,",")) {
        p=trim_ws(p);
        if (!strcmp(p,"atomics")) *flags|=OCC_TARGET_CPU_FEATURE_ATOMICS;
        else if (!strcmp(p,"unaligned_access")) *flags|=OCC_TARGET_CPU_FEATURE_UNALIGNED_ACCESS;
        else if (!strcmp(p,"hardware_float")) *flags|=OCC_TARGET_CPU_FEATURE_HARDWARE_FLOAT;
        else if (!strcmp(p,"hardware_divide")) *flags|=OCC_TARGET_CPU_FEATURE_HARDWARE_DIVIDE;
        else if (!strcmp(p,"privilege_levels")) *flags|=OCC_TARGET_CPU_FEATURE_PRIVILEGE_LEVELS;
        else if (!strcmp(p,"vector")) *flags|=OCC_TARGET_CPU_FEATURE_VECTOR;
        else return 0;
    }
    return 1;
}

int occ_target_profile_load(const char* path, OccTargetProfile* profile,
                            char* error, size_t error_size) {
    FILE* file;
    char line[TARGET_V2_LINE_SIZE], section[32]="";
    char seen[TARGET_V2_SEEN_MAX][TARGET_V2_SEEN_SIZE];
    size_t seen_count=0u;
    unsigned long line_no=0u;
    uint32_t required_target=0u, required_cpu=0u;
    int ok=1;

    enum { T_SCHEMA=1u<<0,T_ID=1u<<1,T_KIND=1u<<2,T_OS=1u<<3,T_RUNTIME=1u<<4,
           T_PACKAGE=1u<<5,T_ABI=1u<<6,T_BINARY=1u<<7,T_ENTRY=1u<<8,T_STACK=1u<<9,
           C_ID=1u<<0,C_KIND=1u<<1,C_ARCH=1u<<2,C_ISA=1u<<3,C_WORD=1u<<4,
           C_ADDR=1u<<5,C_ENDIAN=1u<<6,C_GPR=1u<<7,C_ATOMIC=1u<<8,C_FP=1u<<9,
           C_FLAGS=1u<<10,C_EXT=1u<<11 };

    if (error && error_size) error[0]='\0';
    if (!path || !profile) { fail(error,error_size,"Target path or profile is null."); return 0; }
    file=fopen(path,"r");
    if (!file) { fail(error,error_size,"Could not open Target v2 file: %s",path); return 0; }
    memset(profile,0,sizeof(*profile));
    profile->schema_major=OCC_TARGET_PROFILE_SCHEMA_MAJOR;
    profile->schema_minor=OCC_TARGET_PROFILE_SCHEMA_MINOR;
    profile->profile_kind=OCC_TARGET_PROFILE_FAMILY;
    profile->stack_alignment_bytes=16u;
    profile->device_count=1u;
    profile->devices[0].kind=OCC_TARGET_DEVICE_CPU;
    profile->devices[0].endian=OCC_TARGET_ENDIAN_UNSPECIFIED;

    while (fgets(line,sizeof(line),file)) {
        char* text;
        char* eq;
        char path_key[TARGET_V2_SEEN_SIZE];
        line_no++;
        if (!strchr(line,'\n') && !feof(file)) { fail(error,error_size,"Line %lu exceeds %u bytes.",line_no,TARGET_V2_LINE_SIZE-1u); ok=0; break; }
        remove_comment(line);
        text=trim_ws(line);
        if (!*text) continue;
        if (*text=='[') {
            char* close=strchr(text,']');
            if (!close || close[1]!='\0') { fail(error,error_size,"Invalid section at line %lu.",line_no); ok=0; break; }
            *close='\0';
            if (strcmp(text+1,"target_info") && strcmp(text+1,"cpu")) { fail(error,error_size,"Unknown section at line %lu.",line_no); ok=0; break; }
            strcpy(section,text+1);
            continue;
        }
        eq=strchr(text,'=');
        if (!eq || !*section) { fail(error,error_size,"Expected key = value at line %lu.",line_no); ok=0; break; }
        *eq='\0';
        {
            char* key=trim_ws(text);
            char* value=trim_ws(eq+1);
            size_t i;
            if (!*key || !*value || snprintf(path_key,sizeof(path_key),"%s.%s",section,key)<0) { ok=0; break; }
            for(i=0;i<seen_count;i++) if(!strcmp(seen[i],path_key)) { fail(error,error_size,"Duplicate key %s at line %lu.",path_key,line_no); ok=0; break; }
            if(!ok) break;
            if(seen_count>=TARGET_V2_SEEN_MAX) { fail(error,error_size,"Too many Target keys."); ok=0; break; }
            strcpy(seen[seen_count++],path_key);

            if(!strcmp(section,"target_info")) {
                char value_string[OCC_TARGET_NAME_SIZE];
                uint32_t number;
                if(!strcmp(key,"schema_major")) { if(!read_u32(value,&profile->schema_major)) ok=0; else required_target|=T_SCHEMA; }
                else if(!strcmp(key,"profile_id")) { if(!read_string(value,profile->profile_id,sizeof(profile->profile_id))) ok=0; else required_target|=T_ID; }
                else if(!strcmp(key,"profile_kind")) { if(!read_string(value,value_string,sizeof(value_string))) ok=0; else if(!strcmp(value_string,"family")) profile->profile_kind=OCC_TARGET_PROFILE_FAMILY; else if(!strcmp(value_string,"exact_machine")) profile->profile_kind=OCC_TARGET_PROFILE_EXACT_MACHINE; else ok=0; required_target|=T_KIND; }
                else if(!strcmp(key,"os")) { if(!read_string(value,profile->os,sizeof(profile->os))) ok=0; else required_target|=T_OS; }
                else if(!strcmp(key,"runtime")) { if(!read_string(value,profile->runtime,sizeof(profile->runtime))) ok=0; else required_target|=T_RUNTIME; }
                else if(!strcmp(key,"package_format")) { if(!read_string(value,profile->package_format,sizeof(profile->package_format))) ok=0; else required_target|=T_PACKAGE; }
                else if(!strcmp(key,"abi")) { if(!read_string(value,profile->abi,sizeof(profile->abi))) ok=0; else required_target|=T_ABI; }
                else if(!strcmp(key,"binary_format")) { if(!read_string(value,profile->binary_format,sizeof(profile->binary_format))) ok=0; else required_target|=T_BINARY; }
                else if(!strcmp(key,"entry_point")) { if(!read_string(value,profile->entry_point,sizeof(profile->entry_point))) ok=0; else required_target|=T_ENTRY; }
                else if(!strcmp(key,"stack_alignment_bytes")) { if(!read_u32(value,&number)) ok=0; else { profile->stack_alignment_bytes=number; required_target|=T_STACK; } }
                else if(!strcmp(key,"page_size_bytes")) { if(!read_u32(value,&profile->page_size_bytes)) ok=0; }
                else ok=0;
            } else {
                OccTargetDevice* cpu=&profile->devices[0];
                char str[OCC_TARGET_NAME_SIZE];
                if(!strcmp(key,"id")) { if(!read_string(value,cpu->id,sizeof(cpu->id))) ok=0; else required_cpu|=C_ID; }
                else if(!strcmp(key,"kind")) { if(!read_string(value,str,sizeof(str)) || !parse_device_kind(str,&cpu->kind) || cpu->kind!=OCC_TARGET_DEVICE_CPU) ok=0; else required_cpu|=C_KIND; }
                else if(!strcmp(key,"vendor")) { if(!read_string(value,cpu->vendor,sizeof(cpu->vendor))) ok=0; }
                else if(!strcmp(key,"model")) { if(!read_string(value,cpu->model,sizeof(cpu->model))) ok=0; }
                else if(!strcmp(key,"architecture")) { if(!read_string(value,cpu->architecture,sizeof(cpu->architecture))) ok=0; else required_cpu|=C_ARCH; }
                else if(!strcmp(key,"revision")) { if(!read_string(value,cpu->revision,sizeof(cpu->revision))) ok=0; }
                else if(!strcmp(key,"isa_profile")) { if(!read_string(value,cpu->isa_profile,sizeof(cpu->isa_profile))) ok=0; else required_cpu|=C_ISA; }
                else if(!strcmp(key,"core_count")) { if(!read_u32(value,&cpu->core_count)) ok=0; }
                else if(!strcmp(key,"execution_unit_count")) { if(!read_u32(value,&cpu->execution_unit_count)) ok=0; }
                else if(!strcmp(key,"word_size_bits")) { if(!read_u32(value,&cpu->word_size_bits)) ok=0; else required_cpu|=C_WORD; }
                else if(!strcmp(key,"address_size_bits")) { if(!read_u32(value,&cpu->address_size_bits)) ok=0; else required_cpu|=C_ADDR; }
                else if(!strcmp(key,"endian")) { if(!read_string(value,str,sizeof(str))) ok=0; else if(!strcmp(str,"little")) cpu->endian=OCC_TARGET_ENDIAN_LITTLE; else if(!strcmp(str,"big")) cpu->endian=OCC_TARGET_ENDIAN_BIG; else ok=0; if(ok) required_cpu|=C_ENDIAN; }
                else if(!strcmp(key,"general_register_count")) { if(!read_u32(value,&cpu->general_register_count)) ok=0; else required_cpu|=C_GPR; }
                else if(!strcmp(key,"vector_register_count")) { if(!read_u32(value,&cpu->vector_register_count)) ok=0; }
                else if(!strcmp(key,"predicate_register_count")) { if(!read_u32(value,&cpu->predicate_register_count)) ok=0; }
                else if(!strcmp(key,"vector_width_bits")) { if(!read_u32(value,&cpu->vector_width_bits)) ok=0; }
                else if(!strcmp(key,"atomic_widths")) { if(!add_atomic_widths(value,&cpu->atomic_width_mask)) ok=0; else required_cpu|=C_ATOMIC; }
                else if(!strcmp(key,"fp_formats")) { if(!add_fp_formats(value,&cpu->fp_format_mask)) ok=0; else required_cpu|=C_FP; }
                else if(!strcmp(key,"features")) { if(!add_cpu_flags(value,&cpu->cpu_feature_flags)) ok=0; else required_cpu|=C_FLAGS; }
                else if(!strcmp(key,"isa_extensions")) { if(!read_list(value,cpu->isa_extensions,&cpu->isa_extension_count,OCC_TARGET_MAX_ISA_EXTENSIONS)) ok=0; else required_cpu|=C_EXT; }
                else ok=0;
            }
            if(!ok) { fail(error,error_size,"Invalid or unknown key/value '%s' at line %lu.",key,line_no); break; }
        }
    }
    if(ferror(file)) { fail(error,error_size,"Error reading Target v2 file."); ok=0; }
    fclose(file);
    if(ok && required_target != (T_SCHEMA|T_ID|T_KIND|T_OS|T_RUNTIME|T_PACKAGE|T_ABI|T_BINARY|T_ENTRY|T_STACK)) {
        fail(error,error_size,"Target v2 descriptor is missing required target_info fields."); ok=0;
    }
    if(ok && required_cpu != (C_ID|C_KIND|C_ARCH|C_ISA|C_WORD|C_ADDR|C_ENDIAN|C_GPR|C_ATOMIC|C_FP|C_FLAGS|C_EXT)) {
        fail(error,error_size,"Target v2 descriptor is missing required CPU capability fields."); ok=0;
    }
    if(ok) {
        if(profile->schema_major != OCC_TARGET_PROFILE_SCHEMA_MAJOR) { fail(error,error_size,"Unsupported Target schema major version."); ok=0; }
        else if(occ_target_profile_validate(profile,error,error_size)!=OCC_TARGET_PROFILE_OK) ok=0;
    }
    return ok;
}
