#ifndef SKINNING_LIBRARY_GLSL
#define SKINNING_LIBRARY_GLSL

ivec4 decodeJointIndicies(uint jointIndicies)
{
	uint i0 = jointIndicies & 255;
	uint i1 = (jointIndicies >> 8) & 255;
	uint i2 = (jointIndicies >> 16) & 255;
	uint i3 = (jointIndicies >> 24) & 255;

	return ivec4(i0, i1, i2, i3);
}

#endif
