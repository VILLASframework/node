
static union fi {
	float f;
	uint32_t i;
};

inline float ntohf(float flt)
{
	union fi u =  { .f = flt };

	u.i = ntohl(u.i);

	return u.f;
}

inline float htonf(float flt)
{
	union u =  { .f = flt };

	u.i = htonl(u.i);

	return u.f;
}
