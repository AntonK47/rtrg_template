#ifndef UNIFIED_UPLOAD_HEAP_LIBRARY_GLSL
#define UNIFIED_UPLOAD_HEAP_LIBRARY_GLSL

#define UNIFIED_UPLOAD_HEAP_SET 0
#define UNIFIED_UPLOAD_HEAP_BINDING 2

layout(push_constant) uniform workgroupItemArgumentsBlock
{
	uint argumentsOffset;
	uint count;
} workgroupItemArguments;

#endif