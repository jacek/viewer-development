/** 
 * @file impostorF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * $/LicenseInfo$
 */
 
#version 120

uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
uniform sampler2D specularMap;

void main() 
{
	vec4 col = texture2D(diffuseMap, gl_TexCoord[0].xy);
	gl_FragData[0] = vec4(col.rgb, col.a * 0.005);
	gl_FragData[1] = texture2D(specularMap, gl_TexCoord[0].xy);
	gl_FragData[2] = vec4(texture2D(normalMap, gl_TexCoord[0].xy).xyz, 0.0);
}
