## Sunset Sunrise Calculation

"Time is hard" ~ Weston Boyd, 2019

The CPE needs to calculate sunset/sunrise from latitude and longitude on
a daily basis for automations to trigger sunset/sunrise rules. We
adapted an algorithm that was used prior to 2000, after 2000, its
accuracy decreases. I found that the accuracy is generally just a few
minutes (+/-10) off from the actual sunrise/sunset. It also discounts
any unknowable effects such as light refraction and interference due to
atmospheric events.

There are a bunch of magical numbers that are used as coefficients and
modifiers. Without understanding exactly how the mathematical formulas
work, I can't determine exactly what they mean and where the come from,
but mostly they are used as approximations for certain conditions.

The constants are all pulled from the algorithm posted below. Once we
determined what hour/minute the sunrise or sunset will occur, we take
that data and plug it into a tm struct as the hour and minute. We then
convert it into time_t as UTC time.

The only caveat is that if sunset occurs in the UTC next day from the
concerned local time day, then the formula doesn't recognize that. I am
manually computing that scenario by seeing if time_t for sunset is less
than time_t for sunrise. If sunrise is larger than sunset, then sunset
is adjusted by adding another day

All the different algorithms follow a general format:

1.  Identify the day
2.  Find the exact time for solar noon on that day
3.  Find the angle of declination (angle away from zenith) the sun has
    on that day
4.  Using the declination and solar noon angel, determine when
    sunset/sunrise occur
    


Additional Resources:

http://edwilliams.org/sunrise_sunset_algorithm.htm
https://www.aa.quae.nl/en/reken/zonpositie.html
http://users.electromagnetic.net/bu/astro/sunrise-set.php


Here is the algorithm that was used:

Sunrise/Sunset Algorithm
Source:
	Almanac for Computers, 1990
	published by Nautical Almanac Office
	United States Naval Observatory
	Washington, DC 20392

Inputs:
	day, month, year:      date of sunrise/sunset
	latitude, longitude:   location for sunrise/sunset
	zenith:                Sun's zenith for sunrise/sunset
	  offical      = 90 degrees 50'
	  civil        = 96 degrees
	  nautical     = 102 degrees
	  astronomical = 108 degrees
	
	NOTE: longitude is positive for East and negative for West
        NOTE: the algorithm assumes the use of a calculator with the
        trig functions in "degree" (rather than "radian") mode. Most
        programming languages assume radian arguments, requiring back
        and forth convertions. The factor is 180/pi. So, for instance,
        the equation RA = atan(0.91764 * tan(L)) would be coded as RA
        = (180/pi)*atan(0.91764 * tan((pi/180)*L)) to give a degree
        answer with a degree input for L.


1. first calculate the day of the year

	N1 = floor(275 * month / 9)
	N2 = floor((month + 9) / 12)
	N3 = (1 + floor((year - 4 * floor(year / 4) + 2) / 3))
	N = N1 - (N2 * N3) + day - 30

2. convert the longitude to hour value and calculate an approximate time

	lngHour = longitude / 15
	
	if rising time is desired:
	  t = N + ((6 - lngHour) / 24)
	if setting time is desired:
	  t = N + ((18 - lngHour) / 24)

3. calculate the Sun's mean anomaly
	
	M = (0.9856 * t) - 3.289

4. calculate the Sun's true longitude
	
	L = M + (1.916 * sin(M)) + (0.020 * sin(2 * M)) + 282.634
	NOTE: L potentially needs to be adjusted into the range [0,360) by adding/subtracting 360

5a. calculate the Sun's right ascension
	
	RA = atan(0.91764 * tan(L))
	NOTE: RA potentially needs to be adjusted into the range [0,360) by adding/subtracting 360

5b. right ascension value needs to be in the same quadrant as L

	Lquadrant  = (floor( L/90)) * 90
	RAquadrant = (floor(RA/90)) * 90
	RA = RA + (Lquadrant - RAquadrant)

5c. right ascension value needs to be converted into hours

	RA = RA / 15

6. calculate the Sun's declination

	sinDec = 0.39782 * sin(L)
	cosDec = cos(asin(sinDec))

7a. calculate the Sun's local hour angle
	
	cosH = (cos(zenith) - (sinDec * sin(latitude))) / (cosDec * cos(latitude))
	
	if (cosH >  1) 
	  the sun never rises on this location (on the specified date)
	if (cosH < -1)
	  the sun never sets on this location (on the specified date)

7b. finish calculating H and convert into hours
	
	if if rising time is desired:
	  H = 360 - acos(cosH)
	if setting time is desired:
	  H = acos(cosH)
	
	H = H / 15

8. calculate local mean time of rising/setting
	
	T = H + RA - (0.06571 * t) - 6.622

9. adjust back to UTC
	
	UT = T - lngHour
	NOTE: UT potentially needs to be adjusted into the range [0,24) by adding/subtracting 24

10. convert UT value to local time zone of latitude/longitude
	
	localT = UT + localOffset
  