#if defined(USE_ALPHA_TEST)
uniform vec4 u_AlphaTest; // only x used

bool AlphaTestPassed(float alpha)
{
	if (int(u_AlphaTest.x) == ATEST_GT_0)
	{
		if (alpha <= 0.0)
			return false;
	}
	else if (int(u_AlphaTest.x) == ATEST_LT_128)
	{
		if (alpha >= 0.5)
			return false;
	}
	else if (int(u_AlphaTest.x) == ATEST_GE_128)
	{
		if (alpha < 0.5)
			return false;
	}
	return true;
}
#endif
