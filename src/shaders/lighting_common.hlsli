#ifndef MAX_POINT_LIGHTS
#define MAX_POINT_LIGHTS 8
#endif

struct PointLight {
    float3 position;
    float _pad0;
    float3 color;
    float _pad1;
};

float3 ComputePhongPointLight(
    float3 baseColor,
    float3 worldPos,
    float3 worldNormal,
    float3 viewDir,
    PointLight light,
    float shininess) {
    float3 toLight = light.position - worldPos;
    float distanceToLight = length(toLight);
    float3 lightDir = (distanceToLight > 0.0001f) ? (toLight / distanceToLight) : float3(0.0f, 0.0f, 0.0f);

    float attenuation = clamp(1.0f / max(distanceToLight * distanceToLight, 0.0001f), 0.0f, 1.0f);
    float ndotl = max(dot(worldNormal, lightDir), 0.0f);
    float3 diffuse = baseColor * light.color * ndotl * attenuation;

    float3 reflectDir = reflect(-lightDir, worldNormal);
    float specPower = pow(max(dot(viewDir, reflectDir), 0.0f), shininess);
    float3 specular = light.color * specPower * attenuation;

    return diffuse + specular;
}
