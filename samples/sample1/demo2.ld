name	demo2;

arraysize	20;
textsize	200;

#bind	""			"dotCOBOL"	"TPDEMO2";
#bind	"project3"	"dotCOBOL"	"TPDEMO2";

bind	""			"OpenCOBOL"	"TPDEMO2";
bind	"project3"	"OpenCOBOL"	"TPDEMO2";

multiplex_group	demo2;

home	"=/sample1/demo2";

data	{
	spa		demo2spa;
	window	{
		project3;
	};
};

