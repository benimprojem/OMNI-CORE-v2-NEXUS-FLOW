#include "oir_module.h"
#include <string.h>
static uint16_t g16(const uint8_t*p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t g32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t g64(const uint8_t*p){return (uint64_t)g32(p)|((uint64_t)g32(p+4)<<32);}
static void p16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void p32(uint8_t*p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static void p64(uint8_t*p,uint64_t v){p32(p,(uint32_t)v);p32(p+4,(uint32_t)(v>>32));}
static int utf8(const uint8_t*s,uint32_t n){uint32_t i=0;while(i<n){uint8_t a=s[i];uint32_t cp,k,j;if(!a)return 0;if(a<128){i++;continue;}if(a>=194&&a<=223){cp=a&31;k=1;}else if(a>=224&&a<=239){cp=a&15;k=2;}else if(a>=240&&a<=244){cp=a&7;k=3;}else return 0;if(k>n-i-1)return 0;for(j=1;j<=k;j++){uint8_t b=s[i+j];if((b&192)!=128)return 0;cp=(cp<<6)|(b&63);}if((k==1&&cp<128)||(k==2&&cp<2048)||(k==3&&cp<65536)||(cp>=55296&&cp<=57343)||cp>1114111)return 0;i+=k+1;}return 1;}
static int range(uint32_t a,uint32_t n,uint32_t max){return a<=max&&n<=max-a;}
OirStatus oir_module_open(const uint8_t*p,size_t n,OirModuleView*v){
 uint32_t z[7],i,j;uint16_t schema_minor;uint64_t need=36;const uint8_t*q;
 if(!p||!v)return OIR_STATUS_INVALID_ARGUMENT;memset(v,0,sizeof(*v));
 if(n<36)return OIR_STATUS_INVALID_HEADER;
 schema_minor=g16(p+2);if(g16(p)!=OIR_MODULE_SCHEMA_MAJOR||schema_minor<1u||schema_minor>OIR_MODULE_SCHEMA_MINOR||g32(p+4)!=0)return OIR_STATUS_UNSUPPORTED_VERSION;
 for(i=0;i<7;i++){z[i]=g32(p+8+4*i);if(z[i]>OIR_MODULE_TABLE_COUNT_MAX&&i<6)return OIR_STATUS_INVALID_HEADER;}
 if(z[6]>OIR_MODULE_STRING_BYTES_MAX)return OIR_STATUS_INVALID_HEADER;
 need+=(uint64_t)z[0]*12+(uint64_t)z[1]*20+(uint64_t)z[2]*16+(uint64_t)z[3]*16+(uint64_t)z[4]*24+(uint64_t)z[5]*16+z[6];
 if(need!=n)return OIR_STATUS_LENGTH_MISMATCH;
 q=p+36;v->type_count=z[0];v->types=q;q+=(size_t)z[0]*12;v->function_count=z[1];v->functions=q;q+=(size_t)z[1]*20;
 v->block_count=z[2];v->blocks=q;q+=(size_t)z[2]*16;v->value_count=z[3];v->values=q;q+=(size_t)z[3]*16;
 v->instruction_count=z[4];v->instructions=q;q+=(size_t)z[4]*24;v->operand_count=z[5];v->operands=q;q+=(size_t)z[5]*16;
 v->strings=q;v->strings_length=z[6];if(z[6]&&!utf8(q,z[6]))return OIR_STATUS_INVALID_SECTION;
 for(i=0;i<z[0];i++){const uint8_t*x=v->types+i*12;uint16_t kind=g16(x+4),width=g16(x+6);uint32_t flags=g32(x+8);if(g32(x)!=i+1||kind<1||kind>(schema_minor>=2u?8u:4u)||(flags&~OIR_MODULE_TYPE_KNOWN_FLAGS)!=0u||((kind==OIR_MODULE_TYPE_FUNCTION)?(width!=0||flags!=0):(!width))||(kind!=OIR_MODULE_TYPE_INTEGER&&(flags&OIR_MODULE_TYPE_FLAG_SIGNED)!=0u))return OIR_STATUS_INVALID_SECTION;if((kind==OIR_MODULE_TYPE_BIT&&width!=1u)||(kind==OIR_MODULE_TYPE_BYTE&&width!=8u)||((kind==OIR_MODULE_TYPE_BIT_VECTOR||kind==OIR_MODULE_TYPE_BYTE_BLOCK)&&width!=8u&&width!=16u&&width!=32u&&width!=64u&&width!=128u))return OIR_STATUS_INVALID_SECTION;}
 for(i=0;i<z[1];i++){const uint8_t*x=v->functions+i*20;uint32_t a=g32(x+4),b=g32(x+8);if(g32(x)!=i+1||!b||!range(a,b,z[6])||!utf8(v->strings+a,b)||!g32(x+12)||g32(x+12)>z[0]||!g32(x+16)||g32(x+16)>z[2])return OIR_STATUS_INVALID_SECTION;}
 for(i=0;i<z[2];i++){const uint8_t*x=v->blocks+i*16;if(g32(x)!=i+1||!g32(x+4)||g32(x+4)>z[1]||!range(g32(x+8),g32(x+12),z[4]))return OIR_STATUS_INVALID_SECTION;}
 for(i=0;i<z[3];i++){const uint8_t*x=v->values+i*16;if(g32(x)!=i+1||!g32(x+4)||g32(x+4)>z[0]||g32(x+8)>z[4]||g32(x+12))return OIR_STATUS_INVALID_SECTION;}
 for(i=0;i<z[4];i++){const uint8_t*x=v->instructions+i*24;uint32_t a=g32(x+16),b=g32(x+20),k[255],res=g32(x+12);if(g32(x)!=i+1||!g32(x+4)||g32(x+4)>z[2]||g16(x+10)||res>z[3]||b>255||!range(a,b,z[5]))return OIR_STATUS_INVALID_SECTION;
 for(j=0;j<b;j++){const uint8_t*y=v->operands+(size_t)(a+j)*16;k[j]=g32(y);if(k[j]==OIR_OPERAND_VALUE){if(!g32(y+4)||g32(y+4)>z[3])return OIR_STATUS_INVALID_SECTION;}else if(k[j]==OIR_OPERAND_BLOCK){if(!g32(y+4)||g32(y+4)>z[2])return OIR_STATUS_INVALID_SECTION;}else if(k[j]==OIR_OPERAND_FUNCTION){if(!g32(y+4)||g32(y+4)>z[1])return OIR_STATUS_INVALID_SECTION;}else if(k[j]==OIR_OPERAND_TYPE){if(!g32(y+4)||g32(y+4)>z[0])return OIR_STATUS_INVALID_SECTION;}else if(k[j]!=OIR_OPERAND_INTEGER_IMMEDIATE&&k[j]!=OIR_OPERAND_FLOAT_IMMEDIATE)return OIR_STATUS_INVALID_SECTION;}
 if(!oir_opcode_validate_shape(g16(x+8),k,b,res!=0))return OIR_STATUS_INVALID_SECTION;}
 for(i=0;i<z[5];i++){uint32_t k=g32(v->operands+(size_t)i*16);if(!k||(k&(k-1)))return OIR_STATUS_INVALID_SECTION;}
 return OIR_STATUS_OK;
}
#define DEC16(p) ((uint16_t)((p)[0]|((uint16_t)(p)[1]<<8)))
#define DEC32(p) ((uint32_t)(p)[0]|((uint32_t)(p)[1]<<8)|((uint32_t)(p)[2]<<16)|((uint32_t)(p)[3]<<24))
#define DEC64(p) ((uint64_t)DEC32(p)|((uint64_t)DEC32((p)+4)<<32))
#define GET(fn,T,field,count,sz,body) OirStatus fn(const OirModuleView*v,uint32_t i,T*out){const uint8_t*p;if(!v||!out||i>=v->count||!v->field)return OIR_STATUS_INVALID_ARGUMENT;p=v->field+(size_t)i*(sz);body;return OIR_STATUS_OK;}
GET(oir_module_get_type,OirModuleType,types,type_count,12,out->id=DEC32(p);out->kind=DEC16(p+4);out->bit_width=DEC16(p+6);out->flags=DEC32(p+8))
GET(oir_module_get_function,OirModuleFunction,functions,function_count,20,out->id=DEC32(p);out->name_offset=DEC32(p+4);out->name_length=DEC32(p+8);out->function_type_id=DEC32(p+12);out->entry_block_id=DEC32(p+16))
GET(oir_module_get_block,OirModuleBlock,blocks,block_count,16,out->id=DEC32(p);out->function_id=DEC32(p+4);out->first_instruction=DEC32(p+8);out->instruction_count=DEC32(p+12))
GET(oir_module_get_value,OirModuleValue,values,value_count,16,out->id=DEC32(p);out->type_id=DEC32(p+4);out->defining_instruction_id=DEC32(p+8);out->flags=DEC32(p+12))
GET(oir_module_get_instruction,OirModuleInstruction,instructions,instruction_count,24,out->id=DEC32(p);out->block_id=DEC32(p+4);out->opcode_id=DEC16(p+8);out->reserved=DEC16(p+10);out->result_value_id=DEC32(p+12);out->first_operand=DEC32(p+16);out->operand_count=DEC32(p+20))
GET(oir_module_get_operand,OirModuleOperand,operands,operand_count,16,out->kind=DEC32(p);out->reference_id=DEC32(p+4);out->immediate=DEC64(p+8))
#undef GET
OirStatus oir_module_write(uint8_t*out,size_t cap,const OirModuleInput*in,size_t*written){
 uint64_t total=36;size_t at;uint32_t i;OirModuleView check;
 if(written)*written=0;if(!out||!in||!written)return OIR_STATUS_INVALID_ARGUMENT;
 if(in->type_count>1000000||in->function_count>1000000||in->block_count>1000000||in->value_count>1000000||in->instruction_count>1000000||in->operand_count>1000000||in->strings_length>OIR_MODULE_STRING_BYTES_MAX)return OIR_STATUS_INVALID_ARGUMENT;
 if((in->type_count&&!in->types)||(in->function_count&&!in->functions)||(in->block_count&&!in->blocks)||(in->value_count&&!in->values)||(in->instruction_count&&!in->instructions)||(in->operand_count&&!in->operands)||(in->strings_length&&!in->strings))return OIR_STATUS_INVALID_ARGUMENT;
 total+=(uint64_t)in->type_count*12+(uint64_t)in->function_count*20+(uint64_t)in->block_count*16+(uint64_t)in->value_count*16+(uint64_t)in->instruction_count*24+(uint64_t)in->operand_count*16+in->strings_length;
 if(total>cap)return OIR_STATUS_BUFFER_TOO_SMALL;
 p16(out,OIR_MODULE_SCHEMA_MAJOR);p16(out+2,OIR_MODULE_SCHEMA_MINOR);p32(out+4,0);p32(out+8,in->type_count);p32(out+12,in->function_count);p32(out+16,in->block_count);p32(out+20,in->value_count);p32(out+24,in->instruction_count);p32(out+28,in->operand_count);p32(out+32,in->strings_length);at=36;
 for(i=0;i<in->type_count;i++){p32(out+at,in->types[i].id);p16(out+at+4,in->types[i].kind);p16(out+at+6,in->types[i].bit_width);p32(out+at+8,in->types[i].flags);at+=12;}
 for(i=0;i<in->function_count;i++){const OirModuleFunction*x=in->functions+i;p32(out+at,x->id);p32(out+at+4,x->name_offset);p32(out+at+8,x->name_length);p32(out+at+12,x->function_type_id);p32(out+at+16,x->entry_block_id);at+=20;}
 for(i=0;i<in->block_count;i++){const OirModuleBlock*x=in->blocks+i;p32(out+at,x->id);p32(out+at+4,x->function_id);p32(out+at+8,x->first_instruction);p32(out+at+12,x->instruction_count);at+=16;}
 for(i=0;i<in->value_count;i++){const OirModuleValue*x=in->values+i;p32(out+at,x->id);p32(out+at+4,x->type_id);p32(out+at+8,x->defining_instruction_id);p32(out+at+12,x->flags);at+=16;}
 for(i=0;i<in->instruction_count;i++){const OirModuleInstruction*x=in->instructions+i;p32(out+at,x->id);p32(out+at+4,x->block_id);p16(out+at+8,x->opcode_id);p16(out+at+10,x->reserved);p32(out+at+12,x->result_value_id);p32(out+at+16,x->first_operand);p32(out+at+20,x->operand_count);at+=24;}
 for(i=0;i<in->operand_count;i++){const OirModuleOperand*x=in->operands+i;p32(out+at,x->kind);p32(out+at+4,x->reference_id);p64(out+at+8,x->immediate);at+=16;}
 if(in->strings_length)memcpy(out+at,in->strings,in->strings_length);at+=in->strings_length;
 {OirStatus s=oir_module_open(out,at,&check);if(s!=OIR_STATUS_OK)return s;}*written=at;return OIR_STATUS_OK;
}
#undef DEC16
#undef DEC32
#undef DEC64
