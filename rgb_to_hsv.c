
void RGBtoHSV(unsigned char r, unsigned char g, unsigned char b, unsigned char *h, unsigned char *s, unsigned char *v) {
	float R = r / 255;
	float G = g / 255;
	float B = b / 255;
	float min = min(R, G, B);
	float max = max(R, G, B);
	float delta = max - min;

	// value is max of rgb values
	v = max;

	// if max is > 0, saturation is delta / max
	if(max > 0) {
		s = delta / max;
	}
	// else, max = min, so this color is a shade of gray
	else {
		s = 0;
		h = 0;
		return;
	}

	if(R == max)
		h = ((G - B)/delta + 6) % 6;	// the + 6 % 6 ensures that a negative modulo happens correctly :)
	else if(G == max)
		h = 2 + (B - R)/delta;
	else
		h = 4 + (R - G)/delta;

	// map hue from [0, 6) onto [0, 360)
	h *= 60;
}