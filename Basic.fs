#version 430 core

layout(location=0) out vec4 out_color;
 
in vec4 vertexColor; // Now interpolated across face
in vec4 interPos;
in vec3 interNormal;

struct PointLight {
vec4 pos;
vec4 color;
};

uniform PointLight light;

uniform float metallic;
uniform float roughness;
const float PI = 3.14159265359;

vec3 getFresnelAtAngleZero(vec3 albedo, float metallic)
{
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);
	return F0;
}

vec3 getFresnel(vec3 F0, vec3 L, vec3 H)
{
	float cosAngle = max(0.0, dot(L, H));
	vec3 RF = F0 + ((1 - F0)*pow(1-cosAngle, 5));
	return RF;
}

float getNDF(vec3 H, vec3 N, float roughness)
{
	float a = roughness * roughness;
	float D = pow(a,2) / (PI*pow(pow(dot(N,H),2)*(pow(a,2)-1)+1,2));
	return D;
}

float getSchlickGeo(vec3 B, vec3 N, float roughness)
{
	float k = pow(roughness+1, 2) / 8;
	float G1 = dot(N,B) / (dot(N,B)*(1-k)+k);
	return G1;
}

float getGF(vec3 L, vec3 V, vec3 N, float roughness)
{
	float GL = getSchlickGeo(L, N, roughness);
	float GV = getSchlickGeo(V, N, roughness);
	return GL*GV;
}

void main()
{	
	//vec3 N = interNormal / (sqrt(interNormal[0]^2 + interNormal[1]^2 + interNormal[2]^2));
	vec3 N = normalize(interNormal);

	vec3 L = normalize(vec3(light.pos - interPos));

	float diffuse = max(0, dot(N, L));

	vec3 diffColor = diffuse * vec3(vertexColor * light.color);
	
	//out_color = vec4(diffColor, 1.0);

	vec3 V = normalize(-vec3(interPos));
	vec3 F0 = getFresnelAtAngleZero(vec3(vertexColor), metallic);
	vec3 H = normalize(L+V);
	vec3 F = getFresnel(F0,L,H);
	
	vec3 kS = F;
	vec3 kD = 1.0 - kS;
	kD *= (1.0 - metallic);
	kD *= vec3(vertexColor);
	kD /= PI;

	float NDF = getNDF(H,N,roughness);
	float G = getGF(L,V,N,roughness);
	kS *= NDF * G;
	kS /= (4.0 * max(0, dot(N,L)) * max(0, dot(N,V))) + 0.0001;

	vec3 finalColor =  (kD + kS)*vec3(light.color)*max(0, dot(N,L));
	out_color = vec4(finalColor, 1.0);
}
