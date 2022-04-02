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

void main()
{	
	//vec3 N = interNormal / (sqrt(interNormal[0]^2 + interNormal[1]^2 + interNormal[2]^2));
	vec3 N = normalize(interNormal);

	vec3 L = normalize(vec3(light.pos - interPos));

	float diffuse = max(0, dot(N, L));

	vec3 diffColor = diffuse * vec3(vertexColor * light.color);
	out_color = vec4(diffColor, 1.0);
}
