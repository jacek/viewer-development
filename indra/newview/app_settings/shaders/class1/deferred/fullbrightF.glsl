/** 
 * @file fullbrightF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * $/LicenseInfo$
 */
 
#version 120

#extension GL_ARB_texture_rectangle : enable

uniform sampler2D diffuseMap;
uniform sampler2DRect depthMap;
uniform sampler2D noiseMap;

uniform vec4 shadow_clip;
uniform vec2 screen_res;

vec3 fullbrightAtmosTransport(vec3 light);
vec3 fullbrightScaleSoftClip(vec3 light);

varying vec3 vary_ambient;
varying vec3 vary_directional;
varying vec4 vary_position;
varying vec3 vary_normal;
varying vec3 vary_fragcoord;

uniform mat4 inv_proj;

vec4 getPosition(vec2 pos_screen)
{
	float depth = texture2DRect(depthMap, pos_screen.xy).a;
	vec2 sc = pos_screen.xy*2.0;
	sc /= screen_res;
	sc -= vec2(1.0,1.0);
	vec4 ndc = vec4(sc.x, sc.y, 2.0*depth-1.0, 1.0);
	vec4 pos = inv_proj * ndc;
	pos /= pos.w;
	pos.w = 1.0;
	return pos;
}

void main() 
{
	vec2 frag = vary_fragcoord.xy/vary_fragcoord.z*0.5+0.5;
	frag *= screen_res;
	
	vec3 samp_pos = getPosition(frag).xyz; 
	
	float shadow = 1.0;
	vec4 pos = vary_position;

	vec4 color = texture2D(diffuseMap, gl_TexCoord[0].xy)*gl_Color;
	
	color.rgb = fullbrightAtmosTransport(color.rgb);

	color.rgb = fullbrightScaleSoftClip(color.rgb);

	//gl_FragColor = gl_Color;
	gl_FragColor = color;
	//gl_FragColor = vec4(1,0,1,1);
	
}

