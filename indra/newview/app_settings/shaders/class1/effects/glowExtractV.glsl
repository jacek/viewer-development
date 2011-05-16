/** 
 * @file glowExtractV.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * $/LicenseInfo$
 */

#version 120

void main() 
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	
	gl_TexCoord[0].xy = gl_MultiTexCoord0.xy;
}
