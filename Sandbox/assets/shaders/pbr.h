#define PI 3.1415926535897932384626433832795

float distribution_ggx(vec3 N, vec3 H, float a)
{
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float nom    = a2;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom        = PI * denom * denom;
	
    return nom / denom;
}

float geometry_schlick_ggx(float NdotV, float k)
{
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return nom / denom;
}
  
float geometry_smith(vec3 N, vec3 V, vec3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = geometry_schlick_ggx(NdotV, k);
    float ggx2 = geometry_schlick_ggx(NdotL, k);
	
    return ggx1 * ggx2;
}

vec3 fresnel_schlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 calculate_light(vec3 light_dir, vec3 normal, vec3 view_dir, vec3 albedo, float metallic, float roughness) {
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    vec3 halfway = normalize(light_dir + normal);
    float NDF = distribution_ggx(normal, halfway, roughness);
    float G = geometry_smith(normal, view_dir, light_dir, roughness);
    vec3 F = fresnel_schlick(max(dot(halfway, view_dir), 0.0), F0);
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, view_dir), 0.0) * max(dot(normal, light_dir), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    float cos_phi = max(dot(normal, light_dir), 0.0);
    return (kD * albedo / PI + specular) * cos_phi;
}
