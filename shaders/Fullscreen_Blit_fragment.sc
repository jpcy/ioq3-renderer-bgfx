#include <bgfx_shader.sh>

SAMPLER2D(u_DiffuseMap, 0);

void main()
{
	gl_FragColor = texture2D(u_DiffuseMap, gl_FragCoord.xy * u_viewTexel.xy);
}
