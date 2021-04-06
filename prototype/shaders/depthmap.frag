#version 430 core

out vec4 fcolor;

void main(void)
{
	//float alpha = texture(diffuse, fragment.texcoord).a;
	//if (alpha < 0.5) { discard; }
	fcolor = vec4(1.0);
}
