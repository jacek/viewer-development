/** 
 * @file alphaF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * $/LicenseInfo$
 */
 


#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect depthMap;
uniform sampler2D diffuseMap;


uniform mat4 shadow_matrix[6];
uniform vec4 shadow_clip;
uniform vec2 screen_res;

vec3 atmosLighting(vec3 light);
vec3 scaleSoftClip(vec3 light);

varying vec3 vary_ambient;
varying vec3 vary_directional;
varying vec3 vary_fragcoord;
varying vec3 vary_position;
varying vec3 vary_pointlight_col;

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
	
	vec4 pos = vec4(vary_position, 1.0);
	
	vec4 diff= texture2D(diffuseMap,gl_TexCoord[0].xy);

	vec4 col = vec4(vary_ambient + vary_directional.rgb, gl_Color.a);
	vec4 color = diff * col;
	
	color.rgb = atmosLighting(color.rgb);

	color.rgb = scaleSoftClip(color.rgb);

	color.rgb += diff.rgb * vary_pointlight_col.rgb;

	gl_FragColor = color;
	//gl_FragColor = vec4(1,0,1,1);
	//gl_FragColor = vec4(1,0,1,1)*shadow;
	
}

