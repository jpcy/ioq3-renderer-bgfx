uniform vec4 u_PortalClipEnabled;
uniform vec4 u_PortalPlane;

bool PortalClipped(vec3 position)
{
	return int(u_PortalClipEnabled.x) != 0 && dot(position, u_PortalPlane.xyz) - u_PortalPlane.w < 0.0;
}
