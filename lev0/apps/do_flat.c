#define HIMGCFGFILE "/home/production/img_cnfg_ids"

#define XXX_CANT_OPEN_HIMGCFGFILE -1
#define XXX_CORRUPT_HIMGCFGFILE -2
#define XXX_CANT_FIND_HIMGCFID -3
#define XXX_INVALID_HIMGCFID -4 
#define XXX_INVALID_EXPTIME -5

#define MAXHIMGCFGS 4096
static int overscan_nrows[MAXHIMGCFGS];
static int overscan_ncols[MAXHIMGCFGS];
static int overscan_rowstr[MAXHIMGCFGS];
static int overscan_colstr[MAXHIMGCFGS];
static int overscan_valid[MAXHIMGCFGS];
static int dvals[MAXHIMGCFGS];

#define NBINS 1048576
#define MINOUT -256
#define MAXOUT 1048320
static int hist[NBINS];

#include "aia_despike.c"

///////////////////////////////////
int get_overscan_info(int himgcfid)
{
    static int firstcall = 1;
    static FILE *fp;
    static char line[256];
    int found = 0;
    int id;
    char j[64];

    if (firstcall) {
	fp = fopen(HIMGCFGFILE,"r");
	if (!fp)
	    return XXX_CANT_OPEN_HIMGCFGFILE;
	firstcall = 0;
    }

    rewind(fp);
    while (fgets(line, 256, fp)) {
	if (1 != sscanf(line, "%d", &id))
	    continue;
	if (id != himgcfid)
	    continue;
	if (15 != sscanf(line, "%s%s%s%s%s%s%s%s%s%d%d%d%d%s%d", 
	j,j,j,j,j,j,j,j,j, &overscan_nrows[id], &overscan_ncols[id],
	&overscan_rowstr[id], &overscan_colstr[id], j, &dvals[id]))
	    return XXX_CORRUPT_HIMGCFGFILE;
	found = 1;
	overscan_valid[himgcfid] = 1;
    }

    if (!found)
	return XXX_CANT_FIND_HIMGCFID;

    return 0;
}

///////////////////////////
int do_flat(LEV0LEV1 *info)
{
    int i,j,idx,IDX;
    int nr,nc,r1,r2,c1,c2;
    int status;
    float *flat = info->adataff;
    float *dark = info->adatadark;
    short *in = info->adata0;
    float *out = info->dat1.adata1;
    short tmp;
    int itmp;
    float ftmp;
    double dtmp;
    double exptime;
    double s, s2, s3, s4, ss;
    int npix, min, max, medn;

    exptime = drms_getkey_double(rs0, "EXPTIME", &status);
    if (status || !isfinite(exptime) || exptime <= 0.0)
	return XXX_INVALID_EXPTIME;

    if (info->himgcfid < 0 || info->himgcfid >= MAXHIMGCFGS)
      return XXX_INVALID_HIMGCFID;
    if (!overscan_valid[info->himgcfid]) {
	status = get_overscan_info(info->himgcfid);
	if (status) return status;
    }

    nr = overscan_nrows[info->himgcfid];
    if (nr) {
	r1 = overscan_rowstr[info->himgcfid];
	r2 = r1 + nr;
    } else
	r1 = r2 = 4096;

    nc = overscan_ncols[info->himgcfid];
    if (nc) {
	c1 = overscan_colstr[info->himgcfid];
	c2 = c1 + nc;
    } else
	c1 = c2 = 4096;

    //
    // do overscan statistics
    //
    info->oscnmean = info->oscnrms = DRMS_MISSING_DOUBLE;
    npix = 0;
    s = s2 = 0.0;

    if (nr) {
	for (i=r1; i<r2; ++i) {
	    for (j=0; j<c1; ++j) {
		tmp = in[4096*i+j];
		if (tmp == DRMS_MISSING_SHORT) 
		    continue;
		dtmp = tmp;
		s += dtmp;
		s2 += dtmp*dtmp;
		++npix;
	    }
	    for (j=c2; j<4096; ++j) {
		tmp = in[4096*i+j];
		if (tmp == DRMS_MISSING_SHORT) 
		    continue;
		dtmp = tmp;
		s += dtmp;
		s2 += dtmp*dtmp;
		++npix;
	    }
	}
    }

    if (nc) {
	for (i=0; i<r1; ++i)
	    for (j=c1; j<c2; ++j) {
		tmp = in[4096*i+j];
		if (tmp == DRMS_MISSING_SHORT) 
		    continue;
		dtmp = tmp;
		s += dtmp;
		s2 += dtmp*dtmp;
		++npix;
	    }
	for (i=r2; i<4096; ++i)
	    for (j=c1; j<c2; ++j) {
		tmp = in[4096*i+j];
		if (tmp == DRMS_MISSING_SHORT)
		    continue;
		dtmp = tmp;
		s += dtmp;
		s2 += dtmp*dtmp;
		++npix;
	    }
    }

    if (npix > 1) {
	info->oscnmean = s/npix;
	info->oscnrms = sqrt((s2-s*s/npix) / (npix-1));
    }

    //
    // apply dark, flat correction quadrant by quadrant
    // and squeeze out the overscan rows and columns
    //
    memset(hist, 0, 4*NBINS);
    npix = 0;
    nr /= 2;
    nc /= 2;
    for (i=0; i<r1; ++i) {
	idx = 4096*i;
	IDX = idx + 4096*nr + nc;
	for (j=0; j<c1; ++j) {
	    tmp = in[idx++];
	    if (DRMS_MISSING_SHORT == tmp)
		out[IDX++] = DRMS_MISSING_FLOAT;
	    else {
		ftmp = (tmp - dark[IDX]) / (exptime * flat[IDX]);
		if (ftmp >= MINOUT && ftmp < MAXOUT) {
		    itmp = roundf(ftmp);
		    out[IDX] = ftmp;
		    ++hist[itmp-MINOUT];
		    ++npix;
		} else
		    out[IDX] = DRMS_MISSING_FLOAT;
		++IDX;
	    }
	}
    }
    for (i=0; i<r1; ++i) {
	idx = 4096*i + c2;
	IDX = idx + 4096*nr - nc;
	for (j=c2; j<4096; ++j) {
	    tmp = in[idx++];
	    if (DRMS_MISSING_SHORT == tmp)
		out[IDX++] = DRMS_MISSING_FLOAT;
	    else {
		ftmp = (tmp - dark[IDX]) / (exptime * flat[IDX]);
		if (ftmp >= MINOUT && ftmp < MAXOUT) {
		    itmp = roundf(ftmp);
		    out[IDX] = ftmp;
		    ++hist[itmp-MINOUT];
		    ++npix;
		} else
		    out[IDX] = DRMS_MISSING_FLOAT;
		++IDX;
	    }
	}
    }
    for (i=r2; i<4096; ++i) {
	idx = 4096*i;
	IDX = idx - 4096*nr + nc;
	for (j=0; j<c1; ++j) {
	    tmp = in[idx++];
	    if (DRMS_MISSING_SHORT == tmp)
		out[IDX++] = DRMS_MISSING_FLOAT;
	    else {
		ftmp = (tmp - dark[IDX]) / (exptime * flat[IDX]);
		if (ftmp >= MINOUT && ftmp < MAXOUT) {
		    itmp = roundf(ftmp);
		    out[IDX] = ftmp;
		    ++hist[itmp-MINOUT];
		    ++npix;
		} else
		    out[IDX] = DRMS_MISSING_FLOAT;
		++IDX;
	    }
	}
    }
    for (i=r2; i<4096; ++i) {
	idx = 4096*i + c2;
	IDX = idx - 4096*nr - nc;
	for (j=c2; j<4096; ++j) {
	    tmp = in[idx++];
	    if (DRMS_MISSING_SHORT == tmp)
		out[IDX++] = DRMS_MISSING_FLOAT;
	    else {
		ftmp = (tmp - dark[IDX]) / (exptime * flat[IDX]);
		if (ftmp >= MINOUT && ftmp < MAXOUT) {
		    itmp = roundf(ftmp);
		    out[IDX] = ftmp;
		    ++hist[itmp-MINOUT];
		    ++npix;
		} else
		    out[IDX] = DRMS_MISSING_FLOAT;
		++IDX;
	    }
	}
    }

    info->datavals = npix;
    info->missvals = dvals[info->himgcfid] - npix;

    //
    // fill in blanks around the borders
    //
    if (nr) {
	for (i=0; i<4096*nr; ++i) out[i] = DRMS_MISSING_FLOAT;
	for (i=4096*(4096-nr); i<MAXPIXELS; ++i) out[i] = DRMS_MISSING_FLOAT;
    }
    if (nc) {
	for (i=0; i<4096; ++i) {
	    for (j=0; j<nc; ++j)
		out[i*4096+j] = DRMS_MISSING_FLOAT;
	    for (j=4096-nc; j<4096; ++j)
		out[i*4096+j] = DRMS_MISSING_FLOAT;
	}
    }

    //
    // do statistics
    //
    info->datamin = info->datamax = info->datamedn = DRMS_MISSING_INT;
    info->datamean = info->data_rms = info->dataskew 
	= info->datakurt = DRMS_MISSING_DOUBLE;

    min = 0;
    max = NBINS-1;
    if (npix) {
	while (hist[min] == 0)
	    ++min;
	info->datamin = min + MINOUT;

	while (hist[max] == 0)
	    --max;
	info->datamax = max + MINOUT;

	medn = min;
	i = hist[medn];
	while (2*i < npix)
	    i += hist[++medn];
	info->datamedn = medn + MINOUT;

	s = s2 = s3 = s4 = 0.0;
	for (i=min; i<=max; ++i) {
	    int ii = i + MINOUT;
	    s += (dtmp = ii*hist[i]);
	    s2 += (dtmp *= ii);
	    s3 += (dtmp *= ii);
	    s4 += (dtmp *= ii);
	}
	s /= npix;
	info->datamean = s;
	ss = s*s;
	s2 /= npix;
	s3 /= npix;
	s4 /= npix;
	if (npix > 1) {
	    dtmp = npix * (s2-ss) / (npix-1);
	    info->data_rms = sqrt(dtmp);
	    if (dtmp > 0.0) {
		info->dataskew = (s3 - s * (3*s2 - 2*ss)) / 
		    (dtmp*info->data_rms);
		info->datakurt = (s4 - 4*s*s3 + 3*ss*(2*s2-ss)) / 
		    (dtmp*dtmp) - 3;
	    }
	}
    }

    return 0;
}

///////////////////////////
int do_flat_aia(LEV0LEV1 *info)
{
    int i,j,idx,IDX;
    int nr,nc,r1,r2,c1,c2;
    int status;
    float *flat = info->adataff;
    float *dark = info->adatadark;
    short *in = info->adata0;
    int *out = info->dat1.adata1A;
    short tmp;
    float ftmp;
    double dtmp;
    double s=0.0, s2=0.0, s3=0.0, s4=0.0, ss;
    int npix, min, max, medn;

    if (info->himgcfid < 0 || info->himgcfid >= MAXHIMGCFGS)
      return XXX_INVALID_HIMGCFID;
    if (!overscan_valid[info->himgcfid]) {
	status = get_overscan_info(info->himgcfid);
	if (status) return status;
    }

    nr = overscan_nrows[info->himgcfid];
    if (nr) {
	r1 = overscan_rowstr[info->himgcfid];
	r2 = r1 + nr;
    } else
	r1 = r2 = 4096;
    nr /= 2;

    nc = overscan_ncols[info->himgcfid];
    if (nc) {
	c1 = overscan_colstr[info->himgcfid];
	c2 = c1 + nc;
    } else
	c1 = c2 = 4096;
    nc /= 2;

    memset(hist, 0, 4*NBINS);

    //
    // apply dark, flat correction quadrant by quadrant
    // and squeeze out the overscan rows and columns
    //
    npix = 0;
    for (i=0; i<r1; ++i) {
	idx = 4096*i;
	IDX = idx + 4096*nr + nc;
	for (j=0; j<c1; ++j) {
	    tmp = in[idx++];
	    if (DRMS_MISSING_SHORT == tmp)
		out[IDX++] = DRMS_MISSING_INT;
	    else {
		ftmp = roundf((tmp - dark[IDX]) / flat[IDX]);
		if (ftmp >= MINOUT && ftmp < MAXOUT) {
		    out[IDX] = ftmp;
		    ++hist[out[IDX]-MINOUT];
		    ++npix;
		} else
		    out[IDX] = DRMS_MISSING_INT;
		++IDX;
	    }
	}
    }
    for (i=0; i<r1; ++i) {
	idx = 4096*i + c2;
	IDX = idx + 4096*nr - nc;
	for (j=c2; j<4096; ++j) {
	    tmp = in[idx++];
	    if (DRMS_MISSING_SHORT == tmp)
		out[IDX++] = DRMS_MISSING_INT;
	    else {
		ftmp = roundf((tmp - dark[IDX]) / flat[IDX]);
		if (ftmp >= MINOUT && ftmp < MAXOUT) {
		    out[IDX] = ftmp;
		    ++hist[out[IDX]-MINOUT];
		    ++npix;
		} else
		    out[IDX] = DRMS_MISSING_INT;
		++IDX;
	    }
	}
    }
    for (i=r2; i<4096; ++i) {
	idx = 4096*i;
	IDX = idx - 4096*nr + nc;
	for (j=0; j<c1; ++j) {
	    tmp = in[idx++];
	    if (DRMS_MISSING_SHORT == tmp)
		out[IDX++] = DRMS_MISSING_INT;
	    else {
		ftmp = roundf((tmp - dark[IDX]) / flat[IDX]);
		if (ftmp >= MINOUT && ftmp < MAXOUT) {
		    out[IDX] = ftmp;
		    ++hist[out[IDX]-MINOUT];
		    ++npix;
		} else
		    out[IDX] = DRMS_MISSING_INT;
		++IDX;
	    }
	}
    }
    for (i=r2; i<4096; ++i) {
	idx = 4096*i + c2;
	IDX = idx - 4096*nr - nc;
	for (j=c2; j<4096; ++j) {
	    tmp = in[idx++];
	    if (DRMS_MISSING_SHORT == tmp)
		out[IDX++] = DRMS_MISSING_INT;
	    else {
		ftmp = roundf((tmp - dark[IDX]) / flat[IDX]);
		if (ftmp >= MINOUT && ftmp < MAXOUT) {
		    out[IDX] = ftmp;
		    ++hist[out[IDX]-MINOUT];
		    ++npix;
		} else
		    out[IDX] = DRMS_MISSING_INT;
		++IDX;
	    }
	}
    }

    info->datavals = npix;
    info->missvals = dvals[info->himgcfid] - npix;

    //
    // fill in blanks around the borders
    //
    if (nr) {
	for (i=0; i<4096*nr; ++i) out[i] = DRMS_MISSING_INT;
	for (i=4096*(4096-nr); i<MAXPIXELS; ++i) out[i] = DRMS_MISSING_INT;
    }
    if (nc) {
	for (i=0; i<4096; ++i) {
	    for (j=0; j<nc; ++j)
		out[i*4096+j] = DRMS_MISSING_INT;
	    for (j=4096-nc; j<4096; ++j)
		out[i*4096+j] = DRMS_MISSING_INT;
	}
    }

  if(hmiaiaflg) {                       //aia
    int *oldvalues, *spikelocs, *newvalues, *toprow, *botrow, *tmprow;
    int aia_status, i, level = 7, niter = 3, nbads, nspikes;
    float frac = 0.5;

    /* flip image */
    botrow = l0l1->dat1.adata1A;
    toprow = botrow + 4095*4096;
    tmprow = (int *) malloc(sizeof(int)*4096);
    for (i = 0; i < 2048; i++) {
      memcpy(tmprow, toprow, 16384);
      memcpy(toprow, botrow, 16384);
      memcpy(botrow, tmprow, 16384);
      botrow += 4096;
      toprow -= 4096;
    }
    free(tmprow);

    nbads = ArrayBad->axis[0];
nbads = 0; /* FIXME remove this line when have "blob format" bad pixel files */
    aia_status = aia_despike(l0l1->dat1.adata1A, NULL, 4096, 4096, frac, level,
                 niter, &(l0l1->adatabad), nbads, &nspikes, &oldvalues,
                 &spikelocs, &newvalues);
    set_nspikes(nspikes);
    set_spikelocs(spikelocs);
    set_oldvalues(oldvalues);
    set_newvalues(newvalues);
  } else {
  //dstatus = cosmic_rays(rs, l0l1->dat1.adata1A, l0l1->adatabad, !!TBD, 6.0, !!TBD);
  }


    //
    // do statistics
    //
    info->datamin = info->datamax = info->datamedn = DRMS_MISSING_INT;
    //info->datavals = info->missvals = DRMS_MISSING_INT;
    info->datamean = info->data_rms = info->dataskew 
	= info->datakurt = DRMS_MISSING_DOUBLE;

    min = 0;
    max = NBINS-1;
    if (npix) {
	while (hist[min] == 0)
	    ++min;
	info->datamin = min + MINOUT;

	while (hist[max] == 0)
	    --max;
	info->datamax = max + MINOUT;

	medn = min;
	i = hist[medn];
	while (2*i < npix)
	    i += hist[++medn];
	info->datamedn = medn + MINOUT;

	for (i=min; i<=max; ++i) {
	    int ii = i + MINOUT;
	    s += (dtmp = ii*hist[i]);
	    s2 += (dtmp *= ii);
	    s3 += (dtmp *= ii);
	    s4 += (dtmp *= ii);
	}
	s /= npix;
	info->datamean = s;
	ss = s*s;
	s2 /= npix;
	s3 /= npix;
	s4 /= npix;
	if (npix > 1) {
	    dtmp = npix * (s2-ss) / (npix-1);
	    info->data_rms = sqrt(dtmp);
	    if (dtmp > 0.0) {
		info->dataskew = (s3 - s * (3*s2 - 2*ss)) / 
		    (dtmp*info->data_rms);
		info->datakurt = (s4 - 4*s*s3 + 3*ss*(2*s2-ss)) / 
		    (dtmp*dtmp) - 3;
	    }
	}
    }

    return 0;
}
