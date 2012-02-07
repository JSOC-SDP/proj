function res=track_hmi_movie_loop(fn, t, smpl, cm, fnTs, Mname)
%track_hmi_movie_loop	make movie from tracker metadata files
% 
% res=track_hmi_movie_loop(fn, t, smpl, cm, fnTs, Mname)
% * Make movie of tracking performance.  The output will be a matlab
% "movie" which goes to a file, or which you can export with 
% movie2avi if memory permits (see below).
% * Usage: Controlled by file_loop_cat:
% [fn,M] = file_loop_cat('fulldisk-instant.cat', ...
%                        [0 dt 1], @this_file, 4, cm, fnTs, Mname);
% where 4 gives the spatial reduction (factor of 4 in each direction 
% yields 1024^2 images for HMI), cm is a colormap (try prism2), and 
% fnTs is a pattern for *track* summary mat-files.
% * The optional Mname is a movie filename which is written as an 
% AVI file.  If Mname is not given, the movie frames are returned 
% in res.  This is unworkable for long movies, so if Mname is given,
% just the number of regions present is returned.
% * TODO: Convert to the new movie-exporter (writeVideo).
% * Note fnTs corresponds to mat-files corresponding to individual
% tracks, while the fulldisk instant catfile, given to file_loop_cat, 
% corresponds to the fulldisk images.
% * Using skip=[0 dt 1] here indicates we don't want the images loaded 
% directly, temporal subsampling is dt, and we do need to use the 
% initialization hooks.
% * To obtain the frame labels and to synchronize with the metadata,
% we extract the frame number from the end of the filename `fn'.
% The spatial subsampling is smpl.
% * Alas, we can't have file_loop_cat do the spatial subsampling, 
% because that function just yields up .instant files.
% 
% Inputs:
%   string fn;    -- a .instant file
%   real t;       -- time (given by file_loop_cat)
%   int smpl;
%   real cm(nc, 3);
%   string fnTs
%   opt string Mname = ''
% 
% Outputs:
%   int res; -- a getframe object, or the number of regions found
% 
% See Also: track_hmi_test

% Written by Michael Turmon (turmon@jpl.nasa.gov) on 03 Nov 2009
% Copyright (c) 2009.  All rights reserved.  

% M*: movie names
% prior_t: time of prior image
% Tinfo, FTinx, FTpatch: track/frame info extracted from .mat metadata
persistent Mavi Mavi_num prior_t Tinfo FTinx FTpatch
FPS = 20; % number of frames per second

% 
% Error checking
% 
if all(nargin  ~= [5 6]), error ('Bad input arg number'); end;
% if all(nargout ~= [0 1]), error ('Bad output arg number'); end;
if nargin < 6, Mname = ''; end; % no direct output to movie file
if length(smpl) ~= 1, error('Need one number in smpl'); end;
if size(cm, 2) ~= 3, error('cm is a color map'); end;
if ~ischar(fnTs),  error('fnTs is a filename');  end;
if ~ischar(Mname), error('Mname is a filename'); end;

%
% Initialization
% 
% read fnTs files only once (map from times -> track number)
if isnumeric(fn),
  res = []; % unused, but better if it's there
  if fn < 0, 
    % set up prior time
    prior_t = -Inf; % junk, for now
    % load track data
    [Tinfo,FTinx,FTpatch] = track_summary(fnTs);
    % open streaming movie file
    Mavi_num = 1;
    if ~isempty(Mname),
      if length(strfind(Mname, '%d')) ~= 1,
        error('Mname must contain a %%d pattern');
      end;
      Mavi_name = sprintf(Mname, Mavi_num);
      [junk1,junk2] = mkdir(fileparts(Mavi_name)); % suppress error msgs
      Mavi = avifile(Mavi_name, 'fps', FPS);
    else,
      Mavi = [];
    end;
  end;
  if fn > 0, 
    fprintf('\n');
    if ~isempty(Mavi),
      fprintf('%s: Writing final movie (#%d).\n', mfilename, Mavi_num);
      Mavi_name = Mavi.Filename;
      Mavi = close(Mavi);
      % convert to mp4
      status = avi2mp4(Mavi_name, [], 'clean', 1);
      if status,
        fprintf('Error: failed to convert AVI to MP4 (%s)\n', Mavi_name);
      end;
    end;
    clear Tinfo FTinx FTpatch
    clear Mavi Mavi_num
    clear(mfilename) % clears all persistent vars?
    fprintf('%s: Done.\n', mfilename); 
  end;
  return;
end;


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% Computation
% 
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% check that the track index exists
if isempty(Tinfo) || isempty(FTinx), 
  error('No track info; did you use the initialization hook?');
end;

% we need to write the movie every < 2GB or existing tools fall over
if ~isempty(Mavi),
  % 1 byte per pixel, so count pixels
  avi_length = Mavi.TotalFrames * Mavi.Width * Mavi.Height;
  if avi_length > 1.9 * 2^30,
    disp(sprintf('(avi#%d done)', Mavi_num));
    Mavi_name = Mavi.Filename;
    Mavi = close(Mavi);
    % convert to mp4
    status = avi2mp4(Mavi_name, [], 'clean', 1);
    if status,
      fprintf('Error: failed to convert AVI to MP4 (%s)\n', Mavi_name);
    end;
    Mavi_num = Mavi_num + 1;
    Mavi = avifile(sprintf(Mname, Mavi_num), 'fps', FPS);
  end;
  clear avi_length
end;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% Load image and metadata
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% set up frame_num from filename 
% TODO: looking for a dot is fragile
[p1,p2,p3] = fileparts(fn);
p2_pos = strfind(p2, '.');
if isempty(p2_pos),
  error('Could not extract frame number from fn = %s', fn);
end;
frame_num = str2double(p2(p2_pos(end)+1:end));
clear p1 p2 p3

% load the images un-transposed
%   note: presently not using metadata; we read it all in at once
%   below, it is OK not to get boundary0
keys = {'segment', 'boundary', 'boundary0', 'metadata.name'};
[ims,JJ1,JJ2,JJ3,JJ4,JJ5,fns] = loadinstant(fn, keys, -smpl);
clear JJ*

% get the geometry for this frame
%  (formerly used Tinfo{t}.geom, but fails if no tracks in a frame)
geom = hmidisk(t);

% get the regions for this frame
[rgn,rgnTid,rgnAge,rgnMrg,rgnTag,bound2tk] = ...
    get_regions(frame_num, Tinfo, FTinx, FTpatch);
nR = size(rgn, 1);

% set FLIP
% +1 for no flip (P=0), -1 for flip (P=180)
p0 = geom(5); 
FLIP = 2*(cosd(p0) > 0) - 1;

% load NOAA regions
NOAAar = hmi_noaa_info_interp(t);
% project into our disk geometry
[NOAAx, NOAAy, NOAAz] = hmi_latlon2image([NOAAar.latitudehg ]', ...
                                         [NOAAar.longitudecm]', geom);
NOAAx = round(NOAAx/abs(smpl));
NOAAy = round(NOAAy/abs(smpl));
NOAAz = round(NOAAz/abs(smpl));
NOAAv = (NOAAz > 0); % on-disk filter

% margins
MRGN = 0.01; % [0,1] units
MRGNPIX = floor(MRGN*size(ims, 1)); % pixels

% original region map:
%   offdisk = NaN, ondisk = 0, regions = 1..Nr
% remapped into new integers made to align with the color map
HILITE = 1;  % color index for text, overlays, etc. (intended as white)
LOLITE = 2;  % complementary to above (black)
NEUTA  = 3;  % boring (dark gray)
NEUTB  = 4;  % boring (brighter gray)
FRSTROI= 5;  % first ROI color
TCOLOR = [LOLITE NEUTA NEUTB HILITE]; % text, dark-to-bright
Nt_show = size(cm,1)-(LOLITE+2); % # colors remaining
r2t = [LOLITE NEUTA FRSTROI+rem(bound2tk(:)'-1, Nt_show)]; 
% inputs to r2t remapper come from znan(...) construction, encoded as:
%    offdisk starts as NaN, mapped to 1
%    ondisk starts as 0, mapped to 2
%    blobs start as >=1, mapped to >= FRSTROI
% outputs of r2t mapper: 
%    offdisk => LOLITE, ondisk => NEUTA, blobs => FRSTROI + 0, 1, ...
im1 = r2t(znan(ims(:,:,2),-1)+2);

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% Make a full-disk image with segmentation + boundary info superimposed
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% 1: if we have both posterior and prior boundary, indicate it
if size(ims, 3) == 3,
  % posterior != prior, and on-disk
  roidiff = (ims(:,:,2) ~= ims(:,:,3)) & ~isnan(ims(:,:,2));
  % ran out of good colors
  im1(roidiff) = NEUTB;
end;

% 2: active are LOLITE
im1(ims(:,:,1) == 2) = LOLITE;

% 3: box around each region
% Note: region corners are in the ims() coordinates, unflipped/untransposed
rgnS = floor((rgn-1) / smpl) + 1;
if smpl == 1,
  % L-R and T-B hairlines offset 0 and 1
  tk1 = [1 1 3 3]; % four hairlines...
  tk2 = tk1 + 1;
  del = [0 1 0 1]; % ...offset by 0, 1, 0, 1
  flag = 6; % size of merged-region panel
else,
  % hairlines not offset
  tk1 = [1 3]; % two hairlines
  tk2 = tk1 + 1;
  del = [0 0]; % not offset
  flag = 3;
end;
[M,N] = size(im1);
for r=1:nR,
  % throughout, have to clip to range of indexes to prevent trouble
  im1(rgnS(r,1):rgnS(r,3), range(rgnS(r,tk2) + del, 1, N)) = HILITE;
  im1(range(rgnS(r,tk1) + del, 1, M), rgnS(r,2):rgnS(r,4)) = HILITE;
  if rgnTag(r) < 0,
    % dotted box for a placeholder region
    im1(rgnS(r,1):2:rgnS(r,3), range(rgnS(r,tk2) + del, 1, N)) = NEUTA;
    im1(range(rgnS(r,tk1) + del, 1, M), rgnS(r,2):2:rgnS(r,4)) = NEUTA;
  elseif rgnTag(r) == 0,
    % dotted horizontal lines for a tried-harder region
    im1(range(rgnS(r,tk1) + del, 1, M), rgnS(r,2):2:rgnS(r,4)) = NEUTA;
  end;
  % flag a merged region (clip to image dims)
  im1(range(rgnS(r,1) + [1:flag          ], 1, M), ...
      range(rgnS(r,2) + [1:flag*rgnMrg(r)], 1, N)) = HILITE;
  % flag a new region (clip to image dims)
  im1(range(rgnS(r,1) - [1:flag                     ], 1, M), ...
      range(rgnS(r,2) - [1:flag*double(rgnAge(r)==0)], 1, N)) = HILITE;
end;

% put on NOAA AR markers 
im1 = marker_on_image(im1, [NOAAx(NOAAv) NOAAy(NOAAv)],...
                      '+', [15 3], [], HILITE);

% put on the poles, if they're there
% FIXME: this is temporary!
[NPx, NPy, NPz] = hmi_latlon2image([90;-90], [0;0], geom);
NPx = round(NPx/abs(smpl));
NPy = round(NPy/abs(smpl));
im1 = marker_on_image(im1, [NPx(NPz>0) NPy(NPz>0)], 'x', [15 3], [], HILITE);

% keyboard

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% Make the text overlays
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% We have to rotate the image into final form as-presented (North up)
% before we put the text labels on.
% By contrast, the surrounding boxes have to be done in original
% Matlab coordinates, since they were originally located that way.

% rotate the image up
% FIXME: not sure about this!
im1 = rot90(im1, FLIP);
M = size(im1, 1); % scale info

% load the font for text
if M < 400, 
  DFONT = text_on_image_font('Menlo.ttf', [], [16 0]);
else, 
  DFONT = text_on_image_font('Menlo.ttf', [], [18 0 0 1]);
end;
% make space in im1 for overlay
if     M < 400, pad = [32 96]; 
elseif M < 800, pad = [ 0 64]; 
else            pad = [ 0 32];
end;
% imT is the image-with-text
imT = [zeros(M, pad(1))+LOLITE im1 zeros(M, pad(2))+LOLITE];

% time overlay
t1code = datestr(t, 'yyyy/mm/dd');
t2code = datestr(t, 'HH:MM');
if prior_t > 0,
  t3code = sprintf('dt = %s', datestr(t - prior_t, 'dd+HH:MM'));
else,
  t3code = 'first frame';
end;
% frame number overlay
fnumT = num2str(frame_num, '%06d');
% initial space makes it play nicer with qt player on mac
txt = {'SDO/HMI Tracked AR (HARP)', t1code, t2code, ' ', t3code, ' ', fnumT};
imT = text_on_image(imT, txt, [], TCOLOR, [MRGN MRGN], DFONT);

% nR is the number of tracks, accounting for merges
txt = {...
    sprintf('T = %d', nR), ...
    ' ', ...
    sprintf('! = %d (new)',         nnz(rgnAge == 0)), ...
    sprintf('+ = %d (merge)',       nnz(rgnMrg >  0)), ...
    sprintf('~ = %d (use past)',    nnz(rgnTag == 0)), ...
    sprintf('? = %d (placeholder)', nnz(rgnTag <  0)), ...
      };
% put overlay on
imT = text_on_image(imT, txt, [], TCOLOR, [1-MRGN MRGN], DFONT);

% track number overlay
NC_max = floor(size(imT, 1) / (2*19)); % max track ID's in one column
if nR > 0,
  % compile sizes
  rgnSize = (rgn(:,3)-rgn(:,1)) .* (rgn(:,4) - rgn(:,2)); % NB: orig. units
  [junk,botSize] = sort(rgnSize);
  botSize(rgnMrg > 0) = []; % don't double-indicate
  % compile locations
  UPPER = (rgnS(:,2)+rgnS(:,4)) > M; % center in Northern hemi
  if FLIP < 0, UPPER = ~UPPER; end;
  % now label regions
  rgnID_t = num2str(rgnTid, '%04.0f'); % text format
  rgnID_x = blanks(nR)';
  if length(botSize) > 0,
    rgnID_x(botSize(1))   = 'v'; % smallest
    rgnID_x(botSize(end)) = '^'; % biggest
  end;
  rgnID_x(rgnTag == 0) = '~'; % indicate try-harder tracks
  rgnID_x(rgnTag <  0) = '?'; % indicate placeholder tracks
  rgnID_x(rgnMrg >  0) = '+'; % indicate merges
  rgnID_x(rgnAge == 0) = '!'; % indicate new tracks
  rgnID_t = [rgnID_x rgnID_t];
  % define text blocks
  TBu = columnize(rgnID_t( UPPER,:), NC_max, 1);
  TBd = columnize(rgnID_t(~UPPER,:), NC_max, 0);
  imT = text_on_image(imT, TBu, [], TCOLOR, [MRGN   0.97], DFONT);
  imT = text_on_image(imT, TBd, [], TCOLOR, [1-MRGN 0.97], DFONT);
  % put on the per-track color key
  pix_per_line = 19; % for DFONT
  WID = 20;
  % upper key
  nline = nnz(UPPER);
  track_colors = FRSTROI + rem(rgnTid(UPPER)-1, Nt_show);
  track_block = kron(track_colors, ones(pix_per_line, WID));
  imT(MRGNPIX+[1:(pix_per_line*nline)],end-WID+1:end) = track_block;
  % lower key
  nline = nnz(~UPPER);
  track_colors = FRSTROI + rem(rgnTid(~UPPER)-1, Nt_show);
  track_block = kron(track_colors, ones(pix_per_line, WID));
  imT([end-(pix_per_line*nline)+1:end]-MRGNPIX,end-WID+1:end) = track_block;
end;

% put on NOAA track numbers
% TODO: sort these
if nnz(NOAAv) > 0,
  NOAAlat = [NOAAar.latitudehg]; % all lat's
  NOAAtid = [NOAAar.regionnumber]';
  NOAAspt = [NOAAar.spotcount]';
  % put on northern tags
  NOAAinx = NOAAv & (NOAAlat(:) >= 0); % valid and north
  NOAAtid1 = NOAAtid(NOAAinx); % track id's
  NOAAspt1 = NOAAspt(NOAAinx); % spot counts
  NOAAx1   = NOAAx  (NOAAinx); % x-values
  [junk,NOAAxup] = sort(NOAAx1, 1, 'descend'); % sort valid + north by x
  NBu = num2str(NOAAtid1(NOAAxup), '%05.0f');
  NBs = num2str(NOAAspt1(NOAAxup), ':%2d');
  imT = text_on_image(imT, [NBu NBs], [], TCOLOR, [MRGN 0.88], DFONT);
  % put on southern tags
  NOAAinx = NOAAv & (NOAAlat(:) < 0); % valid and south
  NOAAtid1 = NOAAtid(NOAAinx); % track id's
  NOAAspt1 = NOAAspt(NOAAinx); % spot counts
  NOAAx1   = NOAAx  (NOAAinx); % x-values
  [junk,NOAAxup] = sort(NOAAx1, 1, 'descend'); % sort valid + south by x
  NBd = num2str(NOAAtid1(NOAAxup), '%05.0f'); % text format
  NBs = num2str(NOAAspt1(NOAAxup), ':%2d');
  imT = text_on_image(imT, [NBd NBs], [], TCOLOR, [1-MRGN 0.88], DFONT);
end;


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% Add the frame to the movie
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% the earlier time
prior_t = t;

% movie format
res = im2frame(imT, cm);
if ~isempty(Mavi),
  Mavi = addframe(Mavi, res);
  res = nR; % don't return the big image
end;
return;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% columnize: break text block "t" into columns having at most nlin rows
%
function tp = columnize(t, nlin, up)

npad = 0; % #blanks between columns

[m1,n1] = size(t);
% fits on 1 column
if m1 <= nlin, 
  tp = t; 
  return; 
end;
% put into 2 columns
mf = 2*nlin - m1; % number of fill lines needed
% fail gracefully if too big for 2 columns
if mf < 0,
  mf = 0;
end;
fill = repmat(blanks(n1), [mf 1]); % fill block
pad = repmat(blanks(npad), [nlin 1]); % blank padding
mx = nlin - mf;
% incomplete column goes high or low
if up,
  col1 = strvcat(t(1:mx,:), fill);
else,
  col1 = strvcat(fill, t(1:mx,:));
end;

tp = [col1 pad t(mx+1:end,:)];
return;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% track_summary: load tracks and distill summary information
%
% Tinfo:
% FTinx: 2d sparse, indexed by (frame number, track id)
%   FTinx(f,t) = 1 if frame f contains an instance of track t
% FTpatch: cell array, indexed by (frame number, track id)
%   FTpatch(f,t) = cell array of region numbers of track t in frame f
% Tinfo: cell array of per-track metadata, indexed by track id
%   Tinfo{t} is a struct array, currently containing fields:
%      coords, frame, time, geom, stats, fn, rgn, tag, dims
%   with one entry per image frame.  It does *not* have the chips
%   field, which is the actual bitmaps.

function [Tinfo,FTinx,FTpatch] = track_summary(fnTs)

fnTwild = sprintf(fnTs, '*');
fnTdir  = fileparts(fnTwild);
fnTlist = dir(fnTwild);
nT = length(fnTlist);
if nT == 0,
  error('No tracks matching %s', fnTs);
end;
FTinx = sparse(1, nT);
Tinfo = cell(1, nT);
FTpatch = cell(1, nT);
% loop over tracks
for fn = {fnTlist.name},
  fn1 = fullfile(fnTdir, fn{1});
  t1 = load(fn1);
  % track id number
  tid = t1.tid;
  % all frame numbers in the track
  finx = [t1.rois.frame];
  % set indicator to one for all frames in the track
  FTinx(finx,tid) = 1;
  % set up patch lists
  FTpatch(finx,tid) = {t1.rois.rgn};
  % save per-track metadata 
  % but, store only the sizes of chips
  Tinfo{tid} = rmfield(t1.rois, 'chip');
  % (could not find an easy vectorized way to do this)
  for nc = 1:length(t1.rois),
    Tinfo{tid}(nc).dims = size(t1.rois(nc).chip);
  end;
end;
return;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% get_regions: extract regions from all tracks in a given frame
%
function [rgn,rgnTid,rgnAge,rgnMrg,rgnTag,bound2tk] = get_regions(f1, Tinfo, FTinx, FTpatch)

tlist = find(FTinx(f1,:)); % tracks overlapping current frame
nT = length(tlist);
rgn = zeros(nT,4);
[rgnTid,rgnAge,rgnTag,rgnMrg] = deal(zeros(nT,1)); % ensure columns
bound2tk = [];
for i = 1:nT,
  t1 = tlist(i); % current track
  % FIXME: per-blob track number
  % "all these image-patches mapped into track t1"
  % (is there a way to dimension this in advance?)
  % FIXME: FTpatch can be zero for regions that blink out in a frame
  bound2tk(max(1,FTpatch{f1,t1})) = t1;
  % track id
  rgnTid(i) = t1;
  t1_f1 = find([Tinfo{t1}.frame] == f1); % offset to f1 within t1
  if exist('assert'),
    assert(length(t1_f1) == 1); % take care of old matlabs
  end;
  % age (0 and up)
  rgnAge(i) = (t1_f1 - 1);
  % # merged (0 and up, rgnMrg = -1 if ROI not seen this frame)
  rgnMrg(i) = length(FTpatch{f1,t1}) - 1;
  % tag: -1 => placeholder. 0 => try-harder. 1 => regular roi. 2 => merged.
  rgnTag(i) = Tinfo{t1}(t1_f1).tag;
  % coords
  base = Tinfo{t1}(t1_f1).coords;
  dims = Tinfo{t1}(t1_f1).dims;
  % (no longer find geom here because no tracks => nT == 0 => no geometry)
  % geom = Tinfo{t1}(t1_f1).geom; % same for all i
  rgn(i,:) = [base base+dims-1];
end;
bound2tk = bound2tk(:); % ensure column
% ...if merges, this may not be the case...
% assert(all(bound2tk > 0));
return