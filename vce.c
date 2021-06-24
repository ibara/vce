/*
 * In jurisdictions where Public Domain is recognized:
 *
 * This software was written by Brian Callahan <bcallah@openbsd.org>
 * and released into the Public Domain.
 *
 * Based off Anthony's Editor, Public Domain 1991 by Anthony Howe.
 *
 * -or-
 *
 * In jurisdictions where Public Domain is not recognized:
 *
 * Copyright (c) 2021 Brian Callahan <bcallah@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __unix__
#include <termios.h>

#define BUF (8 * 1024 * 1024)
#endif

#ifdef __cpm__
#include <cpm.h>

#define BUF ((cpm_ramtop - 1) - cpm_ram)
#undef EOF
#define EOF 26
#endif

#ifdef __msdos__
#include <conio.h>

#define BUF (0xffff - 0x8000 - 1)	/* How to do like CP/M? */
#define fgetc(stdin) getch()		/* Don't wait for Return */
#endif

#ifdef ANSI
#ifndef COL_MAX
#define COL_MAX 80
#endif

#ifndef ROW_MAX
#define ROW_MAX 23	/* 23, if using tmux(1) */
#endif
#endif

#define MFLAGS (O_CREAT | O_TRUNC | O_WRONLY)

/*
 * vce - Visual Code Editor
 */

static char *buf, *ebuf;
static char *gap, *egap;
static char modeline[COL_MAX], screen[ROW_MAX - 1][COL_MAX];
static char filename[COL_MAX - 5], response[COL_MAX - 5];

static int col, row = 1, line = 1;
static int idx, page, epage;
static int dirty;

/*
 * Max: 9,999,999
 */
static char *
putn(unsigned int n)
{
	static char num[8];
	char tmp[7];
	int i = 0, j = 0;

	do {
		tmp[i++] = n % 10 + '0';
	} while ((n /= 10) > 0);

	for (i--; i >= 0; i--)
		num[j++] = tmp[i];
	num[j] = '\0';

	return num;
}

static int
strdcat(char *dst, const char *append, unsigned char len)
{
	char *tmp = dst;
	int i = 0;

	for (; *dst; dst++)
		;

	while ((*dst++ = *append++) != '\0') {
		++i;
		if (--len == 0)
			break;
	}

	return i;
}

static int
strdcpy(char *dst, const char *src)
{
	char *tmp = dst;
	int i = 0;

	while ((*dst++ = *src++) != '\0')
		++i;

	return i;
}

static char *
ptr(int offset)
{

	if (offset < 0)
		return buf;

	return (buf + offset + (buf + offset < gap ? 0 : egap - gap));
}

static int
pos(char *pointer)
{

	return (pointer - buf - (pointer < egap ? 0 : egap - gap));
}

static void
movegap(void)
{
	char *p = ptr(idx);

	while (p < gap)
		*--egap = *--gap;

	while (egap < p)
		*gap++ = *egap++;

	idx = pos(egap);
}

static int
prevline(int offset)
{
	char *p;

	while (buf < (p = ptr(--offset)) && *p != '\n')
		;

	return (buf < p ? ++offset : 0);
}

static int
nextline(int offset)
{
	char *p;

	while ((p = ptr(offset++)) < ebuf && *p != '\n')
		;

	return (p < ebuf ? offset : pos(ebuf));
}

static int
adjust(int offset, int column)
{
	char *p;
	int i = 0;

	while ((p = ptr(offset)) < ebuf && *p != '\n' && i < column) {
		i += (*p == '\t') ? 8 - (i & 7) : 1;
		++offset;
	}

	return offset;
}

static void
left(void)
{

	if (0 < idx)
		--idx;
}

static void
right(void)
{

	if (idx < pos(ebuf))
		++idx;
}

static void
up(void)
{

	idx = adjust(prevline(prevline(idx) - 1), col);
}

static void
down(void)
{

	idx = adjust(nextline(idx), col);
}

static void
insert(int ch)
{

	movegap();

	if (ch == '\b' || ch == '\177') {
		if (buf < gap)
			--gap;
	} else if (gap < egap) {
		*gap++ = ((ch == '\r') ? '\n' : ch);
	}

	idx = pos(egap);
}

static unsigned int
get_linecolno(void)
{
	int i = 0;
	unsigned int colno = 0;

	line = 1;
	while (i < idx) {
		if (buf[i++] == '\n') {
			++line;
			colno = 0;
		} else {
			++colno;
		}
	}

	return colno;
}

static void
update_modeline(unsigned int colno)
{
	unsigned int i, rest;

	for (i = 0; i < sizeof(modeline); i++)
		modeline[i] = '\0';

	i = strdcpy(modeline, "VCE: ");

	if (filename[0] != '\0')
		i += strdcat(modeline, filename, COL_MAX > 21 ? 16 : 11);

	if (COL_MAX > 34) {
		while (i < 21)
			i += strdcat(modeline, " ", 1);
		i += strdcat(modeline, "L: ", 3);
		i += strdcat(modeline, putn(line), strlen(putn(line)));

		if (COL_MAX > 48) {
			while (i < 35)
				i += strdcat(modeline, " ", 1);
			i += strdcat(modeline, "C: ", 3);
			i += strdcat(modeline, putn(colno), strlen(putn(colno)));

			if (COL_MAX > 64) {
				while (i < COL_MAX - 13)
					i += strdcat(modeline, " ", 1);

#if defined(__cpm__) || defined(__msdos__)
				i += strdcat(modeline, "  ", 2);
#endif

				i += strdcat(modeline, "Rest: ", 6);

				rest = BUF - (ebuf - egap) - (gap - buf);

#ifdef __unix__
				if (rest < 1000000)
					i += strdcat(modeline, " ", 1);
				if (rest < 100000)
					i += strdcat(modeline, " ", 1);
#endif

				if (rest < 10000)
					i += strdcat(modeline, " ", 1);
				if (rest < 1000)
					i += strdcat(modeline, " ", 1);
				if (rest < 100)
					i += strdcat(modeline, " ", 1);
				if (rest < 10)
					i += strdcat(modeline, " ", 1);
				i += strdcat(modeline, putn(rest),
					strlen(putn(rest)));
			}
		}
	}

	while (i < COL_MAX)
		i += strdcat(modeline, " ", 1);
}

static void
update_display(void)
{
	char *p;
	int i, j, k;

	for (i = 0; i < ROW_MAX - 1; i++) {
		for (j = 0; j < COL_MAX; j++)
			screen[i][j] = ' ';
	}

	if (idx < page)
		page = prevline(idx);

	if (epage <= idx) {
		page = nextline(idx);
		i = ((page == pos(ebuf)) ? ROW_MAX - 3 : ROW_MAX - 1);
		while (0 < i--)
			page = prevline(page - 1);
	}

	i = 0;
	j = 0;
	epage = page;

	while (1) {
		if (idx == epage) {
			row = i;
			col = j;
		}
		p = ptr(epage);
		if ((ROW_MAX - 1) <= i || ebuf <= p)
			break;
		if (*p != '\r') {
			if (*p == '\n') {
				screen[i][j++] = ' ';
			} else if (*p == '\t') {
				k = 8 - (j & 7);
				while (k--)
					screen[i][j++] = ' ';
			} else {
				screen[i][j++] = *p;
			}
		}
		if (*p == '\n' || COL_MAX <= j) {
			++i;
			j = 0;
		}
		++epage;
	}

	update_modeline(get_linecolno());

#ifdef ANSI
	write(1, "\033[2J\033[H\033[7m", 11);
	write(1, modeline, sizeof(modeline));
	write(1, "\033[0m", 4);

	for (i = 0; i < ROW_MAX - 1; i++) {
		write(1, "\033[", 2);
		write(1, putn(i + 2), strlen(putn(i + 2)));
		write(1, ";1H", 3);
		write(1, screen[i], sizeof(screen[i]));
	}

	write(1, "\033[", 2);
	write(1, putn(row + 2), strlen(putn(row + 2)));
	write(1, ";", 1);
	write(1, putn(col + 1), strlen(putn(col + 1)));
	write(1, "H", 1);
#endif
}

static char *
get_response(void)
{
	int ch, i, j = 0;

	for (i = 0; i < sizeof(response); i++)
		response[i] = '\0';

	for (i = 0; i < COL_MAX; i++)
		modeline[i] = '\0';

	i = strdcpy(modeline, "VCE: ");
	while (i < COL_MAX)
		i += strdcat(modeline, " ", 1);

#ifdef ANSI
	write(1, "\033[H\033[7m", 7);
	write(1, modeline, sizeof(modeline));
	write(1, "\033[1;6H", 6);

	while ((ch = fgetc(stdin)) != '\n' && ch != '\r') {
		if (ch == '\b' || ch == '\177') {
			if (j == 0)
				continue;

			for (i = 0; i < sizeof(modeline); i++)
				modeline[i] = '\0';

			i = strdcpy(modeline, "VCE: ");
			while (i < COL_MAX)
				i += strdcat(modeline, " ", 1);
			write(1, "\033[H", 3);
			write(1, modeline, sizeof(modeline));

			response[--j] = '\0';

			write(1, "\033[1;6H", 6);
			write(1, response, strlen(response));
		} else {
			if (j == COL_MAX - 6)
				continue;

			if (!isalnum(ch) && ch != '.' && ch != '_')
				continue;

			for (i = 0; i < sizeof(modeline); i++)
				modeline[i] = '\0';

			i = strdcpy(modeline, "VCE: ");
			while (i < COL_MAX)
				i += strdcat(modeline, " ", 1);
			write(1, "\033[H", 3);
			write(1, modeline, sizeof(modeline));

			response[j++] = ch;

			write(1, "\033[1;6H", 6);
			write(1, response, strlen(response));
		}
	}

	write(1, "\033[0m", 4);

	write(1, "\033[", 2);
	write(1, putn(row + 2), strlen(putn(row + 2)));
	write(1, ";", 1);
	write(1, putn(col + 1), strlen(putn(col + 1)));
	write(1, "H", 1);
#endif

	return (j == 0) ? NULL : response;
}

static void
message(const char *msg)
{
	int i;

	for (i = 0; i < COL_MAX; i++)
		modeline[i] = '\0';

	i = strdcpy(modeline, "VCE: ");
	i += strdcat(modeline, msg, strlen(msg));
	while (i < COL_MAX)
		i += strdcat(modeline, " ", 1);

#ifdef ANSI
	write(1, "\033[H\033[7m", 7);
	write(1, modeline, sizeof(modeline));
	write(1, "\033[0m", 4);
#endif

	while ((i = fgetc(stdin)) != '\n') {
		if (i == '\r')
			break;
	}
}

static void
save_file(void)
{
	char *bp;
	int fd, i, saveidx = idx;

	if (filename[0] == '\0') {
		if (get_response() == NULL) {
			message("no filename");
			return;
		}
	}

	for (i = 0; i < sizeof(filename); i++)
		filename[i] = '\0';

	for (i = 0; response[i] != '\0'; i++)
		filename[i] = response[i];

	if ((fd = open(filename, MFLAGS, 0644)) == -1) {
		message("failed open");
		return;
	}

	idx = 0;

	movegap();

#if defined(__unix__) || defined(__msdos__)
	write(fd, egap, ebuf - egap);
#elif defined(__cpm__)
	bp = egap;
	while (*bp != EOF) {
		if (bp == ebuf)
			break;

		if (*bp == '\0')
			break;

		if (*bp == '\n')
			write(fd, "\r\n", 2);
		else
			write(fd, bp, 1);

		++bp;
	}
#endif

	close(fd);

	idx = saveidx;

	dirty = 0;

	message("save ok");
}

static int
getn(const char *str)
{
	int i = 0;

	while (*str >= '0' && *str <= '9')
		i = (i * 10) + (*str++ - '0');

	return i;
}

static void
goto_line(void)
{
	char *str;
	int i = 0, target = 0;

	if ((str = get_response()) != NULL)
		target = getn(str);

	idx = 0;
	while (++i < target)
		idx = adjust(nextline(idx), 0);
}

static void
init_buf(void)
{
	char *bp;

#if defined(__unix__)
	if ((buf = calloc(1, BUF)) == NULL) {
		fprintf(stderr, "vce: unable to create buffer\n");
		exit(1);
	}
#elif defined(__cpm__)
	buf = (char *) cpm_ram;

	for (bp = buf; bp < (char *) cpm_ramtop; bp++)
		*bp = '\0';
#elif defined(__msdos__)
	buf = (char *) 0x8000;

	for (bp = buf; bp < buf + BUF; bp++)
		*bp = '\0';
#endif

	gap = buf;
	ebuf = buf + BUF;
	egap = ebuf;
}

int
main(int argc, char *argv[])
{
	char *bp;
	int ch, done = 0, fd, i;

#ifdef __unix__
	struct termios term_new, term_old;
#endif

	if (argc > 2) {
		fprintf(stderr, "usage: vce [file]\n");
		exit(1);
	}

	if (COL_MAX < 16 || ROW_MAX < 2) {
		fprintf(stderr, "vce: error: terminal too small\n");
		exit(1);
	}

	init_buf();

#if defined(__unix__)
	tcgetattr(0, &term_old);
	memcpy(&term_new, &term_old, sizeof(struct termios));
	term_new.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	term_new.c_oflag &= ~(OPOST);
	term_new.c_cflag |= (CS8);
	term_new.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	if (tcsetattr(0, TCSANOW, &term_new) == -1) {
		fprintf(stderr, "vce: could not set terminal\n");
		exit(1);
	}
#elif defined(__cpm__) || defined(__msdos__)
	write(1, "\033[12h", 5);
#endif

	if (argc == 2) {
		for (i = 0; i < strlen(argv[1]); i++)
			filename[i] = argv[1][i];
		filename[i] = '\0';

		if ((fd = open(filename, O_RDONLY)) == -1)
			goto out;

#if defined(__unix__) || defined(__msdos__)
		gap += read(fd, buf, BUF);
#elif defined(__cpm__)
		bp = buf;
		while (read(fd, &ch, 1) > 0) {
			if (bp == ebuf)
				break;

			if (ch == EOF || ch == '\0')
				break;

			if (ch != '\r') {
				*bp++ = ch;
				++gap;
			}
		}
#endif

		if (gap < buf)
			gap = buf;

		close(fd);
	}

out:
	idx = 0;
	while (!done) {
		update_display();

		ch = fgetc(stdin);
		switch (ch) {
		case '\004': /* ^D */
			right();
			break;
		case '\005': /* ^E */
			up();
			break;
		case '\014': /* ^L */
			update_display();
			break;
		case '\023': /* ^S */
			left();
			break;
		case '\030': /* ^X */
			down();
			break;
		case '\033': /* ESC */
			ch = fgetc(stdin);
			switch (ch) {
#if defined(ANSI) && !defined(__msdos__)
			case '[': /* Arrow keys */
				ch = fgetc(stdin);
				switch(ch) {
				case 'A':
					up();
					break;
				case 'B':
					down();
					break;
				case 'C':
					right();
					break;
				case 'D':
					left();
				}
				break;
#endif
			case 'g':
				goto_line();
				break;
			case 'q':
				done = 1;
				break;
			case 's':
				save_file();
				break;
			case 'v':
				message("Version 0.9");
			}
			break;
		default:
			insert(ch);
		}
	}

#if defined(__unix__)
	if (tcsetattr(0, TCSANOW, &term_old) == -1) {
		fprintf(stderr, "vce: could not return terminal\n");
		exit(1);
	}
#endif

#ifdef ANSI
#if defined(__cpm__) || defined(__msdos__)
	write(1, "\033[12l", 5);
#endif
	write(1, "\033[H\033[2J\033[H", 10);
#endif

	return 0;
}
