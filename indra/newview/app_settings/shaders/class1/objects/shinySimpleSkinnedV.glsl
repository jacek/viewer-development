/** 
 * @file shinySimpleSkinnedV.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

#version 120

vec4 calcLighting(vec3 pos, vec3 norm, vec4 color, vec4 baseCol);
void calcAtmospherics(vec3 inPositionEye);
mat4 getObjectSkinnedTransform();

attribute vec4 object_weight;

void main()
{
	mat4 mat = getObjectSkinnedTransform();
	
	mat = gl_ModelViewMatrix * mat;
	vec3 pos = (mat*gl_Vertex).xyz;
	
	vec4 norm = gl_Vertex;
	norm.xyz += gl_Normal.xyz;
	norm.xyz = (mat*norm).xyz;
	norm.xyz = normalize(norm.xyz-pos.xyz);
		
	vec3 ref = reflect(pos.xyz, -norm.xyz);

	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
	gl_TexCoord[1] = gl_TextureMatrix[1]*vec4(ref,1.0);

	calcAtmospherics(pos.xyz);

	vec4 color = calcLighting(pos.xyz, norm.xyz, gl_Color, vec4(0.));
	gl_FrontColor = color;
	
	gl_Position = gl_ProjectionMatrix*vec4(pos, 1.0);
}
