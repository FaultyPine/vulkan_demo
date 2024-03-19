#version 450

struct CloudData 
{
    vec4 cameraOffset;
    //     ( pointMagnitudeScalar, cloudDensityNoiseScalar, cloudDensityNoiseFreq, cloudDensityPointLengthFreq )
    vec4 cloudDensityParams;
    vec4 sun_dir_and_time;
};

layout(binding = 0) uniform uniform_buffer_obj {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 resolution;
    CloudData cloud;
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
    return ubo.cloud.sun_dir_and_time.w;
}
vec3 get_sun_dir()
{
    return normalize(ubo.cloud.sun_dir_and_time.xyz);
}

void main() 
{
    float time = gettime();
    vec2 resolution = ubo.resolution.xy;
    vec2 uv = gl_FragCoord.xy / resolution;
    uv.y = 1.0 - uv.y; // vulkan doesn't flip - opengl does. pretending to be opengl rn
    uv -= 0.5;
    uv.x *= resolution.x / resolution.y;
    vec4 cloud = cloud_main(uv, time, resolution);
    outColor = cloud;
}






#define MAX_STEPS 30
#define SURF_EPSILON 0.001
#define MAX_DIST 100.0

mat2 rotate2D(float a) 
{
    float sa = sin(a);
    float ca = cos(a);
    return mat2(ca, -sa, sa, ca);
}

mat3 rotate3DX(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat3(1,  0, 0,
                0,  c, -s,
                0,  s, c);
}

mat3 rotate3DY(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat3(c,  0,  s,
                0,  1,  0,
                -s, 0,  c);
}

mat3 rotate3DZ(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat3(c, -s, 0,
                s, c,  0,
                0, 0,  1);
}

vec3 repeat(vec3 p, float c) 
{
    return mod(p,c) - 0.5 * c; // (0.5 *c centers the tiling around the origin)
}

float remap(float value, float min1, float max1, float min2, float max2) 
{
    return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

float fbm(vec3 p) 
{
    float res = 0.0;
    float amp = 0.8;
    float freq = 1.5;
    for(int i = 0; i < 12; i++) {
        res += amp * noise(p * 0.8);
        amp *= 0.5;
        freq *= 1.05;
        p = p * freq * rotate3DZ(PI / 4.0);
    }
    return res;
}


// NOTE: sdf's conventionally return negative inside an object and positive outside the object
float sdfBox(vec3 p, vec3 b) 
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdfSphere(vec3 p, float radius)
{
    return length(p) - radius;
}

vec4 sdfMin(vec4 obj1, vec4 obj2)
{
    return obj1.w < obj2.w ? obj1 : obj2;
}

// NOTE: to self, to translate you must move in the *opposite* direction to the desired position
// imagine yourself as a point in a raymarched scene with a sphere: if you take 2 steps to the right, the sphere will appear to you two steps further to the left
// scaling is also odd.  
vec4 scene(vec3 point)
{
    float fbm = fbm(point);
    float time = gettime();
    vec3 objColor = vec3(1.0, 0, 0);
    float objSdf = sdfSphere(point + vec3(10,0,0)*sin(time), 1.0);
    vec4 obj1 = vec4(objColor, objSdf);
    float distanceSmoothedNoise = fbm * (1/length(point));
    vec3 planeColor = vec3(246,215,176)/255.0;
    planeColor = mix(planeColor, vec3(225,191,146)/255.0, fbm);
    vec4 plane = vec4(planeColor, point.y + 1.0 + distanceSmoothedNoise);

    vec4 distance = sdfMin(obj1, plane);
    return distance;
}

float cloudDensitySample(vec3 point)
{
    float timescroll = gettime() * 0.3;
    float pointMagnitudeScalar = ubo.cloud.cloudDensityParams.x;
    float cloudDensityNoiseScalar = ubo.cloud.cloudDensityParams.y;
    float cloudDensityNoiseFreq = ubo.cloud.cloudDensityParams.z;
    float cloudDensityPointLengthFreq = ubo.cloud.cloudDensityParams.w;
    // this is generally a sphere shape. Note the similarity to sdfSphere
    // except here we modulate some of the calculations and use fractal brownian motion for our "sphere" radius
    // need to invert some operations though since we aren't measuring distance to a surface
    // we are returning the *density* at some point which is positive in our shape, and <= 0 outside the shape
    point.y -= 10.0;
    float sphere = SURF_EPSILON - length(point * cloudDensityPointLengthFreq) * pointMagnitudeScalar;
    float noise = fbm(point * cloudDensityNoiseFreq + timescroll) * cloudDensityNoiseScalar;
    return sphere + noise;
}

float get_light_transmittance(vec3 rayOrigin, vec3 rayDirection, int cloudSampleCount, int lightSampleCount, float lightSampleMaxZ)
{
    float lightTransmittance = 1.0;
    float absorption = 100.0;
    float stepSize = lightSampleMaxZ / float(lightSampleCount);
    // step from the point along a ray casted towards the light
    vec3 lightPoint = rayOrigin;
    for (int i = 0; i < lightSampleCount; i++)
    {
        float densityLight = cloudDensitySample(lightPoint);
        // If densityLight is over 0.0, the ray is in an object.
        if (densityLight > 0.0)
        {
            float tmpl = densityLight / float(cloudSampleCount);
            lightTransmittance *= 1.0 - (tmpl * absorption);
        }
        if (lightTransmittance <= 0.01)
        {
            break;
        }
        lightPoint += rayDirection * stepSize;
    }
    return lightTransmittance;
}

vec4 cloud_march(vec3 rayOrigin, vec3 rayDirection, float sceneDepth, vec3 lightDir)
{
    #define USE_LIGHT 1
    float time = gettime();
    float transmittance = 1.0;
    float absorption = 100.0;

    const int cloudSampleCount = 64;
    // dividing the max distance our ray can go into discrete MAX_STEPS number of steps
    // zStep is how far each of those steps is
    float zStep = MAX_DIST / float(cloudSampleCount);

    vec4 color = vec4(0,0,0,1);
    vec3 point = rayOrigin;
    for (int i = 0; i < cloudSampleCount; i++)
    {
        float densitySample = cloudDensitySample(point);
        if (densitySample > 0.0)
        {
            // we are in the volume & have some density here
            // TODO: RESEARCH THIS. IDK WHATS GOING ON HERE
            // tmp is something like the integral for the sample point(s)
            float tmp = densitySample / float(cloudSampleCount); // ????
            transmittance *= 1.0 - (tmp * absorption); // ???
            // ----------------------------------------
            if (transmittance <= 0.01)
            {
                // ray has been absorbed by the cloud
                break;
            }

            float opacity = 50.0;
            float k = opacity * tmp * transmittance;
            vec3 cloudColor = mix(vec3(1), vec3(0.5), densitySample);
            vec3 cloudBase = cloudColor * k;
            //vec4 cloudBase = vec4(cloudColor, densitySample);
            vec4 cloudColorIQ = vec4( mix( vec3(1.0,0.93,0.84), vec3(0.25,0.3,0.4), densitySample ), densitySample );

            #if USE_LIGHT
            // this step along the ray contributes to our final cloud color
            float lightTransmittance = 
                get_light_transmittance(point, lightDir, cloudSampleCount, 3, 15.0);
            float opacityLight = 80.0;
            float kl = opacityLight * tmp * transmittance * lightTransmittance;
            vec3 lightColor = vec3(1.0, 0.7, 0.4);
            vec3 cloudLightColor = lightColor * kl;
            #else
            vec3 cloudLightColor = vec3(0);
            #endif

            color.rgb += cloudBase.rgb + cloudLightColor;
        }
        point += rayDirection * zStep; // step forward through the ray
        //zStep += length(point - rayOrigin); // as we get farther from the camera, step farther since details don't matter as much
        if (length(point) >= sceneDepth) break; // depth test
    }

    return color;
}

// courtesey of IQ
// idea is to cast a ray from a point on a surface toward the light dir
// and take steps through the scene to see if we intersect anything
// if we do intersect, we are in shadow.
float softShadows(vec3 ro, vec3 rd, float mint, float maxt, float k) 
{
    float resultingShadowColor = 1.0;
    float t = mint;
    for (int i = 0; i < MAX_STEPS && t < maxt; i++) 
    {
        float h = scene(ro + rd*t).w;
        if(h < SURF_EPSILON)
        {
            return 0.0;
        }
        resultingShadowColor = min(resultingShadowColor, k*h/t );
        t += h;
    }
    return resultingShadowColor;
}

vec3 getNormal(in vec3 p) 
{
    vec2 e = vec2(.01, 0);
    vec3 n = scene(p).w - vec3(
        scene(p-e.xyy).w,
        scene(p-e.yxy).w,
        scene(p-e.yyx).w);
    return normalize(n);
}

vec4 raymarch(vec3 rayOrigin, vec3 rayDirection)
{
    vec4 dO = vec4(0.0);
    for (int i = 0; i < MAX_STEPS; i++)
    {
        vec3 point = rayOrigin + (rayDirection * dO.w);
        vec4 distanceToSurface = scene(point);
        dO += distanceToSurface;
        if (dO.w > MAX_DIST || distanceToSurface.w < SURF_EPSILON)
        { // if we've gone too far or have hit a surface
            break;
        }   
    }
    return dO;
}


// lookAt
mat3 camera(vec3 rayOrigin, vec3 target)
{
    vec3 cw = normalize(rayOrigin - target);
    vec3 cp = vec3(0.0, 1.0, 0.0);
    vec3 cu = cross(cw, cp);
    vec3 cv = cross(cu, cw);
    return mat3(cu, cv, cw);
}

vec4 cloud_main(vec2 uv, float time, vec2 resolution)
{
    vec4 color = vec4(vec3(0),1);
    // raymarching setup
    vec3 rayOrigin = normalize(ubo.cloud.cameraOffset.xyz) * 40.0;
    // rays in every direction on the screen along the negative z axis
    vec3 cameraTarget = vec3(0,1,0);
    mat3 cam = camera(rayOrigin, cameraTarget);
    vec3 rayDirection = normalize(cam * normalize(vec3(uv, -1.0)));
    vec3 lightDir = get_sun_dir();

    vec3 bg1 = vec3(138.0/255, 231.0/255, 241.0/255);
    vec3 bg2 = vec3(84.0/255, 163.0/255, 245.0/255);
    //color.rgb = mix(bg1, bg2, uv.y);

    vec4 raymarchResult = raymarch(rayOrigin, rayDirection);
    vec3 sceneColor = raymarchResult.rgb;
    float distToSurf = raymarchResult.w;
    if (distToSurf < MAX_DIST) // if we ended up hitting a surface
    {
        float lightDist = 60;
        vec3 pointOnSurface = rayOrigin + (rayDirection * distToSurf);

        vec3 ambient = vec3(0.01);
        float diffuseLightIntensity = 0.05;
        vec3 diffuseLightColor = vec3(1) * diffuseLightIntensity;
        vec3 normal = getNormal(pointOnSurface);
        float lightRadius = 70.0;
        vec3 lightPos = lightDist * -lightDir;
        float attenuation = 1.0-remap(min(lightRadius, length(lightPos - pointOnSurface)), 0, lightRadius, 0, 1);
        vec3 diffuse = max(dot(normal, lightDir), 0.0) * attenuation * diffuseLightColor;
        // cast a ray from the surface point toward the light direction. Intersection = in shadow, no intersection = in light
        float shadows = softShadows(pointOnSurface, lightDir, 0.1, 5.0, 64.0);
        color.rgb = sceneColor * (diffuse + ambient) * max(0.3, shadows);
    }

    vec4 cloud = cloud_march(rayOrigin, rayDirection, distToSurf, lightDir);
    color += cloud;

    // background sky
    float sun = clamp(dot(lightDir,rayDirection), 0.0, 1.0);
    //color.rgb -= 0.6*vec3(0.90,0.75,0.95)*rayDirection.y;
	//color.rgb += 0.2*vec3(1.00,0.60,0.10)*pow( sun, 8.0 );

    return color;
}

