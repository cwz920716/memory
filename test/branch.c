int A[10];

int extra(int x);

int small(int s)
{
	int a = 100 + s;
	int b = 25 * s;
	A[0] = 1;
	if (a > b)
		b = extra(b);
	else if (a < b)
		b = extra(a);
	else
		b = extra(0);
	if (a > b) {
		if (a > b + 10)
			b = extra(b);
		else
			b = extra(b+10);
	} else if (a < b)
		b = extra(a);
	else
		b = extra(0);
	return b;
}