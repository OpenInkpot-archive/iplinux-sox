/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "st.h"
#include "version.h"
#include "patchlvl.h"
#include <string.h>
#include <ctype.h>
#include <signal.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * util.c.
 * Incorporate Jimen Ching's fixes for real library operation: Aug 3, 1994.
 * Redo all work from scratch, unfortunately.
 * Separate out all common variables used by effects & handlers,
 * and utility routines for other main programs to use.
 */

/* export flags */
/* FIXME: To be moved inside of fileop structure per handler. */
int verbose = 0;	/* be noisy on stderr */

/* FIXME:  These functions are user level concepts.  Move them outside
 * the ST library. 
 */
char *myname = 0;

void
report(const char *fmt, ...) 
{
	va_list args;

	if (! verbose)
		return;

	fprintf(stderr, "%s: ", myname);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}


void
warn(const char *fmt, ...) 
{
	va_list args;

	fprintf(stderr, "%s: ", myname);
	va_start(args, fmt);

	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

void
fail(const char *fmt, ...) 
{
	va_list args;
	extern void cleanup();

	fprintf(stderr, "%s: ", myname);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	cleanup();
	exit(2);
}


/* Warning: no error checking is done with errstr.  Be sure not to
 * go over the array limit ourself!
 */
void
st_fail(ft_t ft, int errno, const char *fmt, ...)
{
	va_list args;

	ft->st_errno = errno;

	va_start(args, fmt);
	vsprintf(ft->st_errstr, fmt, args);
	va_end(args);
}

int strcmpcase(s1, s2)
char *s1, *s2;
{
	while(*s1 && *s2 && (tolower(*s1) == tolower(*s2)))
		s1++, s2++;
	return *s1 - *s2;
}

/*
 * Check that we have a known format suffix string.
 */
void
st_gettype(formp)
ft_t formp;
{
	char **list;
	int i;

	if (! formp->filetype)
fail("Must give file type for %s file, either as suffix or with -t option",
formp->filename);
	for(i = 0; st_formats[i].names; i++) {
		for(list = st_formats[i].names; *list; list++) {
			char *s1 = *list, *s2 = formp->filetype;
			if (! strcmpcase(s1, s2))
				break;	/* not a match */
		}
		if (! *list)
			continue;
		/* Found it! */
		formp->h = &st_formats[i];
		return;
	}
	if (! strcmpcase(formp->filetype, "snd")) {
		verbose = 1;
		report("File type '%s' is used to name several different formats.", formp->filetype);
		report("If the file came from a Macintosh, it is probably");
		report("a .ub file with a sample rate of 11025 (or possibly 5012 or 22050).");
		report("Use the sequence '-t .ub -r 11025 file.snd'");
		report("If it came from a PC, it's probably a Soundtool file.");
		report("Use the sequence '-t .sndt file.snd'");
		report("If it came from a NeXT, it's probably a .au file.");
		fail("Use the sequence '-t .au file.snd'\n");
	}
	fail("File type '%s' of %s file is not known!",
		formp->filetype, formp->filename);
}

/*
 * Check that we have a known effect name.
 */
void
st_geteffect(effp)
eff_t effp;
{
	int i;

	for(i = 0; st_effects[i].name; i++) {
		char *s1 = st_effects[i].name, *s2 = effp->name;
		while(*s1 && *s2 && (tolower(*s1) == tolower(*s2)))
			s1++, s2++;
		if (*s1 || *s2)
			continue;	/* not a match */
		/* Found it! */
		effp->h = &st_effects[i];
		return;
	}
	/* Guido Van Rossum fix */
	fprintf(stderr, "%s: Known effects: ",myname);
	for (i = 1; st_effects[i].name; i++)
		fprintf(stderr, "%s ", st_effects[i].name);
	fprintf(stderr, "\n");
	fail("Effect '%s' is not known!", effp->name);
}

/*
 * File format routines 
 */

void st_copyformat(ft, ft2)
ft_t ft, ft2;
{
	int noise = 0, i;
	double factor;

	if (ft2->info.rate == 0) {
		ft2->info.rate = ft->info.rate;
		noise = 1;
	}
	if (ft2->info.size == -1) {
		ft2->info.size = ft->info.size;
		noise = 1;
	}
	if (ft2->info.encoding == -1) {
		ft2->info.encoding = ft->info.encoding;
		noise = 1;
	}
	if (ft2->info.channels == -1) {
		ft2->info.channels = ft->info.channels;
		noise = 1;
	}
	if (ft2->comment == NULL) {
		ft2->comment = ft->comment;
		noise = 1;
	}
	/* 
	 * copy loop info, resizing appropriately 
	 * it's in samples, so # channels don't matter
	 */
	factor = (double) ft2->info.rate / (double) ft->info.rate;
	for(i = 0; i < ST_MAX_NLOOPS; i++) {
		ft2->loops[i].start = ft->loops[i].start * factor;
		ft2->loops[i].length = ft->loops[i].length * factor;
		ft2->loops[i].count = ft->loops[i].count;
		ft2->loops[i].type = ft->loops[i].type;
	}
	/* leave SMPTE # alone since it's absolute */
	ft2->instr = ft->instr;
}

void st_cmpformats(ft, ft2)
ft_t ft, ft2;
{
}

/* check that all settings have been given */
void st_checkformat(ft) 
ft_t ft;
{
	if (ft->info.rate == 0)
		fail("Sampling rate for %s file was not given\n", ft->filename);
	if ((ft->info.rate < 100) || (ft->info.rate > 999999L))
		fail("Sampling rate %lu for %s file is bogus\n", 
			ft->info.rate, ft->filename);
	if (ft->info.size == -1)
		fail("Data size was not given for %s file\nUse one of -b/-w/-l/-f/-d/-D", ft->filename);
	if (ft->info.encoding == -1 && ft->info.size != ST_SIZE_FLOAT)
		fail("Data encoding was not given for %s file\nUse one of -s/-u/-U/-A", ft->filename);
	/* it's so common, might as well default */
	if (ft->info.channels == -1)
		ft->info.channels = 1;
	/*	fail("Number of output channels was not given for %s file",
			ft->filename); */
}

static ft_t ft_queue[2];

void
sigint(s)
int s;
{
    if (s == SIGINT) {
	if (ft_queue[0])
	    ft_queue[0]->file.eof = 1;
	if (ft_queue[1])
	    ft_queue[1]->file.eof = 1;
    }
}

void
sigintreg(ft)
ft_t ft;
{
    if (ft_queue[0] == 0)
	ft_queue[0] = ft;
    else
	ft_queue[1] = ft;
    signal(SIGINT, sigint);
}
