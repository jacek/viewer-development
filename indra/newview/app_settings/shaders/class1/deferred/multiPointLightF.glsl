/** 
 * @file multiPointLightF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * $/LicenseInfo$
 */



#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect depthMap;
uniform sampler2DRect diffuseRect;
uniform sampler2DRect specularRect;
uniform sampler2DRect normalMap;
uniform samplerCube environmentMap;
uniform sampler2D noiseMap;
uniform sampler2D lightFunc;


uniform vec3 env_mat[3];
uniform float sun_wash;

uniform int light_count;

#define MAX_LIGHT_COUNT		16
uniform vec4 light[MAX_LIGHT_COUNT];
uniform vec4 light_col[MAX_LIGHT_COUNT];

varying vec4 vary_fragcoord;
uniform vec2 screen_res;

uniform float far_z;

uniform mat4 inv_proj;

vec4 getPosition(vec2 pos_screen)
{
	float depth = texture2DRect(depthMap, pos_screen.xy).r;
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
	vec2 frag = (vary_fragcoord.xy*0.5+0.5)*screen_res;
	vec3 pos = getPosition(frag.xy).xyz;
	if (pos.z < far_z)
	{
		discard;
	}
	
	vec3 norm = texture2DRect(normalMap, frag.xy).xyz;
	norm = vec3((norm.xy-0.5)*2.0,norm.z); // unpack norm
	norm = normalize(norm);
	vec4 spec = texture2DRect(specularRect, frag.xy);
	vec3 diff = texture2DRect(diffuseRect, frag.xy).rgb;
	float noise = texture2D(noiseMap, frag.xy/128.0).b;
	vec3 out_col = vec3(0,0,0);
	vec3 npos = normalize(-pos);

	// As of OSX 10.6.7 ATI Apple's crash when using a variable size loop
	for (int i = 0; i < MAX_LIGHT_COUNT; ++i)
	{
		bool light_contrib = (i < light_count);
		
		vec3 lv = light[i].xyz-pos;
		float dist2 = dot(lv,lv);
		dist2 /= light[i].w;
		if (dist2 > 1.0)
		{
			light_contrib = false;
		}
		
		float da = dot(norm, lv);
		if (da < 0.0)
		{
			light_contrib = false;
		}
		
		if (light_contrib)
		{
			lv = normalize(lv);
			da = dot(norm, lv);
					
			float fa = light_col[i].a+1.0;
			float dist_atten = clamp(1.0-(dist2-1.0*(1.0-fa))/fa, 0.0, 1.0);
			dist_atten *= noise;

			float lit = da * dist_atten;
			
			vec3 col = light_col[i].rgb*lit*diff;
			//vec3 col = vec3(dist2, light_col[i].a, lit);
			
			if (spec.a > 0.0)
			{
				//vec3 ref = dot(pos+lv, norm);
				
				float sa = dot(normalize(lv+npos),norm);
				
				if (sa > 0.0)
				{
					sa = texture2D(lightFunc,vec2(sa, spec.a)).a * min(dist_atten*4.0, 1.0);
					sa *= noise;
					col += da*sa*light_col[i].rgb*spec.rgb;
				}
			}
			
			out_col += col;
		}
	}
	
	if (dot(out_col, out_col) <= 0.0)
	{
		discard;
	}
	
	gl_FragColor.rgb = out_col;
	gl_FragColor.a = 0.0;
	
	//gl_FragColor = vec4(0.1, 0.025, 0.025/4.0, 0.0);
}
