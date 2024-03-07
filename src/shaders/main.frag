#version 450

layout(binding = 0) uniform uniform_buffer_obj {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 sun_dir_and_time;
    vec4 resolution;
} ubo;

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;

//#define CLOUD_PREMADE

#ifdef CLOUD_PREMADE
vec4 cloud_main(vec2 uv, float time);
#else
vec4 cloud_main(vec2 uv, float time, vec2 resolution); // mine
#endif

#define PI 3.14159265359

float hash(float n)
{
    return fract(sin(n) * 43758.5453);
}
float noise(in vec3 x)
{
    vec3 p = floor(x);
    vec3 f = fract(x);
    
    f = f * f * (3.0 - 2.0 * f);
    
    float n = p.x + p.y * 57.0 + 113.0 * p.z;
    
    float res = mix(mix(mix(hash(n +   0.0), hash(n +   1.0), f.x),
                        mix(hash(n +  57.0), hash(n +  58.0), f.x), f.y),
                    mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
                        mix(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z);
    return res;
}

float gettime()
{
    return ubo.sun_dir_and_time.w;
}

void main() 
{
    float time = ubo.sun_dir_and_time.w;
    vec2 resolution = ubo.resolution.xy;
    vec2 uv = gl_FragCoord.xy / resolution;
    uv.y = 1.0 - uv.y; // vulkan doesn't flip - opengl does. pretending to be opengl rn
    uv -= 0.5;
    uv.x *= resolution.x / resolution.y;

    #ifdef CLOUD_PREMADE
    vec4 cloud = cloud_main(fragUV, time);
    #else 
    vec4 cloud = cloud_main(uv, time, resolution);
    #endif
    outColor = cloud;
}








#ifndef CLOUD_PREMADE

#define MAX_STEPS 50
#define SURF_EPSILON 0.001
#define MAX_DIST 100.0

mat2 rotate2D(float a) 
{
    float sa = sin(a);
    float ca = cos(a);
    return mat2(ca, -sa, sa, ca);
}

vec3 repeat(vec3 p, float c) 
{
    return mod(p,c) - 0.5 * c; // (0.5 *c centers the tiling around the origin)
}

float fbm(vec2 p) 
{
    float res = 0.0;
    float amp = 0.8;
    float freq = 1.5;
    for(int i = 0; i < 12; i++) {
        res += amp * noise(vec3(p * 0.8, 0.0));
        amp *= 0.5;
        freq *= 1.05;
        p = p * freq * rotate2D(PI / 4.0);
    }
    return res;
}

float sdfBox(vec3 p, vec3 b) 
{
  vec3 q = abs(p) - b;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdfSphere(vec3 p, float radius)
{
    return length(p) - radius;
}

// NOTE: to self, to translate you must move in the *opposite* direction to the desired position
// imagine yourself as a point in a raymarched scene with a sphere: if you take 2 steps to the right, the sphere will appear to you two steps further to the left
// scaling is also odd.  
float scene(vec3 p)
{
    float fbm = fbm(p.xz);
    float time = gettime();
    vec3 spherepoint = p + vec3(sin(time), cos(time), sin(time));
    float sphere = sdfSphere(spherepoint, 1.0);
    float noise = fbm * (1/length(p));
    float plane = p.y + 1.0 + noise;

    float distance = min(sphere, plane);
    return distance;
}

// courtesey of IQ
// idea is to cast a ray from a point on a surface toward the light dir
// and take steps through the scene to see if we intersect anything
// if we do intersect, we are in shadow.
float softShadows(vec3 ro, vec3 rd, float mint, float maxt, float k ) {
  float resultingShadowColor = 1.0;
  float t = mint;
  for(int i = 0; i < 50 && t < maxt; i++) {
      float h = scene(ro + rd*t);
      if( h < 0.001 )
          return 0.0;
      resultingShadowColor = min(resultingShadowColor, k*h/t );
      t += h;
  }
  return resultingShadowColor ;
}

vec3 getNormal(in vec3 p) 
{
    vec2 e = vec2(.01, 0);
    vec3 n = scene(p) - vec3(
        scene(p-e.xyy),
        scene(p-e.yxy),
        scene(p-e.yyx));
    return normalize(n);
}

float raymarch(vec3 rayOrigin, vec3 rayDirection)
{
    float dO = 0.0;
    vec3 color = vec3(0.0);
    for (int i = 0; i < MAX_STEPS; i++)
    {
        vec3 p = rayOrigin + (rayDirection * dO);
        float distanceToSurface = scene(p);
        dO += distanceToSurface;
        if (dO > MAX_DIST || distanceToSurface < SURF_EPSILON)
        { // if we've gone too far or have hit a surface
            break;
        }   
    }
    return dO;
}


vec4 cloud_main(vec2 uv, float time, vec2 resolution)
{
    // raymarching setup
    vec3 rayOrigin = vec3(0, 0, 5.0); // camera
    // rays in every direction on the screen along the negative z axis
    vec3 rayDirection = normalize(vec3(uv, -1.0)); 
    float distToSurf = raymarch(rayOrigin, rayDirection);

    vec3 color = vec3(0.0);
    if (distToSurf < MAX_DIST)
    {
        vec3 pointOnSurface = rayOrigin + (rayDirection * distToSurf);
        vec3 lightPos = vec3(-10.0, 10.0, 10.0); // sun pos
        vec3 normal = getNormal(pointOnSurface);
        vec3 lightDir = normalize(lightPos - pointOnSurface);
        float diffuse = max(dot(normal, lightDir), 0.0);
        vec3 ambient = vec3(0.01);
        // cast a ray from the surface point toward the light direction. Intersection = in shadow, no intersection = in light
        float shadows = softShadows(pointOnSurface, lightDir, 0.1, 5.0, 64.0);
        color = vec3(1.0) * (diffuse + ambient) * shadows;
    }

    return vec4(color, 1.0);
}













#else // CLOUD_PREMADE

#define USE_LIGHT 1

mat3 m = mat3( 0.00,  0.80,  0.60,
              -0.80,  0.36, -0.48,
              -0.60, -0.48,  0.64);

///
/// Fractal Brownian motion.
///
/// Refer to:
/// EN: https://thebookofshaders.com/13/
/// JP: https://thebookofshaders.com/13/?lan=jp
///
float fbm(vec3 p)
{
    float f;
    f  = 0.5000 * noise(p); p = m * p * 2.02;
    f += 0.2500 * noise(p); p = m * p * 2.03;
    f += 0.1250 * noise(p);
    return f;
}

//////////////////////////////////////////////////

///
/// Sphere distance function.
///
/// But this function return inverse value.
/// Normal dist function is like below.
/// 
/// return length(pos) - 0.1;
///
/// Because this function is used for density.
///
float sampleDensity(in vec3 pos)
{
    return 0.1 - length(pos) * 0.05 + fbm(pos * 0.3);
}

float scene(in vec3 pos)
{
    return sampleDensity(pos);
}


vec3 getNormal(in vec3 p) 
{
    vec2 e = vec2(.01, 0);
    vec3 n = scene(p) - vec3(
        scene(p-e.xyy),
        scene(p-e.yxy),
        scene(p-e.yyx));
    return normalize(n);
}

///
/// Create a camera pose control matrix.
///
mat3 camera(vec3 ro, vec3 ta)
{
    vec3 cw = normalize(ta - ro);
    vec3 cp = vec3(0.0, 1.0, 0.0);
    vec3 cu = cross(cw, cp);
    vec3 cv = cross(cu, cw);
    return mat3(cu, cv, cw);
}


vec4 cloud_main(vec2 uv, float time)
{
    vec2 mo = vec2(time * 0.1, cos(time * 0.25));
    
    // Camera
    float camDist = 40.0;
    
    // target
    vec3 ta = vec3(0.0, 1.0, 0.0);
    
    // Ray origin
    //vec3 ori = vec3(sin(time) * camDist, 0, cos(time) * camDist);
    vec3 ro = camDist * normalize(vec3(cos(2.75 - 3.0 * mo.x), 0.7 - 1.0 * (mo.y - 1.0), sin(2.75 - 3.0 * mo.x)));
    
    float targetDepth = 1.3;
    
    // Camera pose.
    mat3 c = camera(ro, ta);
    vec3 dir = c * normalize(vec3(uv, targetDepth));
    
    // For raymarching const values.
    const int sampleCount = 64;
    const int sampleLightCount = 6;
    const float eps = 0.01;
    
    // Raymarching step settings.
    float zMax = 40.0;
    float zstep = zMax / float(sampleCount);
    
    float zMaxl = 20.0;
    float zstepl = zMaxl / float(sampleLightCount);
    
    // Easy access to the ray origin
    vec3 p = ro;
    
    // Transmittance
    float T = 1.0;
    
    // Substantially transparency parameter.
    float absorption = 100.0;
    
    // Light Direction
    vec3 sun_direction = ubo.sun_dir_and_time.xyz;
    
    // Result of culcration
    vec4 color = vec4(0.0);
    
    for (int i = 0; i < sampleCount; i++)
    {
        // Using distance function for density.
        // So the function not normal value.
        // Please check it out on the function comment.
        float density = scene(p);
        
        // The density over 0.0 then start cloud ray marching.
        // Why? because the function will return negative value normally.
        // But if ray is into the cloud, the function will return positive value.
        if (density > 0.0)
        {
            // Let's start cloud ray marching!
            
            // why density sub by sampleCount?
            // This mean integral for each sampling points.
            float tmp = density / float(sampleCount);
            
            T *= 1.0 - (tmp * absorption);
            
            // Return if transmittance under 0.01. 
            // Because the ray is almost absorbed.
            if (T <= 0.01)
            {
                break;
            }
            
            #if USE_LIGHT == 1
            // Light scattering
            
            // Transmittance for Light
            float Tl = 1.0;
            
            // Start light scattering with raymarching.
            
            // Raymarching position for the light.
            vec3 lp = p;
            
            // Iteration of sampling light.
            for (int j = 0; j < sampleLightCount; j++)
            {
                float densityLight = scene(lp);
                
                // If densityLight is over 0.0, the ray is stil in the cloud.
                if (densityLight > 0.0)
                {
                    float tmpl = densityLight / float(sampleCount);
                    Tl *= 1.0 - (tmpl * absorption);
                }
                
                if (Tl <= 0.01)
                {
                    break;
                }
                
                // Step to next position.
                lp += sun_direction * zstepl;
            }
            #endif
            
            // Add ambient + light scattering color
            float opaity = 50.0;
            float k = opaity * tmp * T;
            vec4 cloudColor = vec4(1.0);
            vec4 col1 = cloudColor * k;
            
            #if USE_LIGHT == 1
            float opacityl = 30.0;
            float kl = opacityl * tmp * T * Tl;
            vec4 lightColor = vec4(1.0, 0.7, 0.9, 1.0);
            vec4 col2 = lightColor * kl;
            #else
            vec4 col2 = vec4(0.0);
            #endif
            
            color += col1 + col2;
        }
        
        p += dir * zstep;
    }
    
    vec3 bg = mix(vec3(0.3, 0.1, 0.8), vec3(0.7, 0.7, 1.0), 1.0 - (uv.y + 1.0) * 0.5);
    color.rgb += bg;
    
	return color;
}
#endif // CLOUD_PREMADE