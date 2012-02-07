function out = text_on_image(img, txt, icolor, tcolor, pos, font)
%text_on_image	overlay text onto image
% 
% out = text_on_image(img, txt, icolor, tcolor, pos, font)
% * Overlay text (txt) on image (img).  If img is empty, just the
% overlay is returned.  Each overlay pixel is a scalar value,
% so it can serve as an index or a weight on an RGB color.
% * Both uint8 truecolor images and indexed images are supported, with 
% somewhat different icolor/tcolor interpretations.
% * Non-printable ascii characters are turned into spaces.  Character 
% matrices and cell arrays of strings are stacked.
% * The image color is controlled by icolor; the text color by tcolor.
% For truecolor images, scalar colors are expanded to RGB triples of the 
% given color, and everything must be between 0 and 1.
% For indexed images, icolor and tcolor are the (integer) colormap
% indexes.  (Real values like NaN and Inf are "correctly" treated
% by Matlab, also.)  
% * Additionally, for scalar images, vector tcolor is OK.  It is
% interpreted as defining a mapping from the overlay range [0,1] to
% a color index tcolor.  This can give smoother outlines, even if 
% tcolor is only of length 3.
% * For truecolor images, the color of `out' is the *sum* of the 
% background (image), and the foreground (letter-sequence bitmaps), where:
%   bg = cat(3, icolor(1)*img(R), icolor(2)*img(G), icolor(3)*img(B))
%   fg = cat(3, tcolor(1)*bitmap, tcolor(2)*bitmap, tcolor(3)*bitmap)
% For blue text, use tcolor = [0 0 1].  For a black background, use 
% icolor = 0.  To keep the background intact, use icolor = 1.  Off-scale
% colors (outside of [0,255] after linear combination) are clipped to
% stay in-range.
% * For indexed images, and simple scalar icolor/tcolor, the color
% index of `out' is icolor if the overlay is < 128, or tcolor otherwise.
% For vector tcolor, we map the overlay range thru tcolor.  In all
% cases, if the overlay is "0", icolor is placed in the output.  If
% icolor is empty, *the output receives the image value* where the
% overlay equals 0.  This is the default for indexed images.
% * The position of the letter bitmaps within the image is given in scaled
% coordinates by pos(1:2).  Use (1,1) for lower right, (0,0) for upper
% left, and (0.5,0.5) for centered.  The third component of pos, if
% given, encodes the rotation.  The most useful value is 1, which gives
% standard vertical text.
% * The font is a font structure which is output from 
% text_on_image_font, or text_on_image_font_old.  See those routines 
% for more.
% 
% Inputs:
%   uint8 img(m,n,3) or double img(m,n)
%   char txt(mt,nt) or cell txt{mt}
%   opt real icolor(3) or (1)
%      (truecolor: icolor = [0 0 0]. indexed: icolor = [].)
%   opt real tcolor(3) or (1) or (nc)
%      (truecolor: tcolor = [0.7 0.7 0.7]. indexed: icolor = 1.)
%   opt real pos(3) = [1 1 0]  -- in [0,1] x [0,1] x {-2,-1,0,1,2}
%   opt struct font = text_on_image_font_old('dejavu24');
% 
% Outputs:
%   uint8 out(m,n,3) or double out(m,n)
%    
% See Also: text_on_image_font for font construction

% Written by Michael Turmon (turmon@jpl.nasa.gov) on 25 Sep 09.
% Copyright (c) 2009.  All rights reserved.

% This is the string we used, taken from `man ascii'.
% ' !"#$%&''()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~'
% We only support contiguous dictionaries at the moment.

% 
% Error checking
% 
if all(nargin  ~= [2 3 4 5 6]), error ('Bad input arg number'); end
% if all(nargout ~= [0 1]), error ('Bad output arg number'); end  

% truecolor or indexed?
P = size(img, 3); % 1 or 3
% set up args
if nargin < 6, 
  font = text_on_image_font_old('dejavu24');
end;
if nargin < 5 || isempty(pos), pos = [1 1]; end;
if length(pos) == 2, pos = [pos 0]; end; % plug in final 0
if nargin < 4, 
  % (text) truecolor: gray. indexed: "color #2"
  if P == 3, tcolor = 0.7; else, tcolor = 2; end;
end;
if nargin < 3, 
  % (image) truecolor: black.  indexed: "color #1"
  if P == 3, icolor = 0.0; else, icolor = []; end;
end;
% convert cell text to string
if iscell(txt), txt = strvcat(txt); end;
% expand scalar colors to length-P
if P == 3,
  if length(icolor) == 1,
    icolor = icolor + zeros(1,P);
  end;
  if length(tcolor) == 1,
    tcolor = tcolor + zeros(1,P);
  end;
end;
% some more checking
if ~isstruct(font), error('Font must be a struct'); end;
if length(pos) ~= 3, error('Pos must have 2 or 3 elements'); end;
if length(tcolor) ~= P && P == 3, error('Bad tcolor'); end; %P=1:vector OK
if length(icolor) ~= P && ~isempty(icolor), error('Bad icolor'); end;
if ~ischar(txt), error('Text must be a string'); end;
if all(P ~= [1 3]), error('Need empty, indexed, or RGB image'); end;

%
% Computation
% 
bitmaps = font.bitmaps;
dict = font.dict;

% bitmap letters are Mb by Nb
[Mb,Nb,junk] = size(bitmaps);
[Mt,Nt] = size(txt);

%% Make compute dictionary index for each char in txt
% find index within dict of the char we will call blank
blankchar = find(dict == ' ', 1); % first space
if isempty(blankchar), blankchar = 1; end;
% distance from txt to dict (Ntxt x Ndict)
dist = abs(repmat(dict, [Mt*Nt 1]) - repmat(txt(:), [1 length(dict)]));
[exact,inx] = min(dist, [], 2); % min along dict axis
% create indexes into dict
indexes = zeros(size(txt)) + blankchar; % all blank for now
indexes(exact == 0) = inx(exact == 0); % only replace exact matches

% bitmaps for each letter, stacked along third dim
letters = bitmaps(:,:,indexes(:));
% reshape so a multi-line txt can be permuted
block = reshape(letters, [Mb Nb Mt Nt]);
% permute and reshape into the text as displayed; rotate as desired
block = rot90(reshape(permute(block, [1 3 2 4]), [Mb*Mt Nb*Nt]), pos(3));

% if empty image, just return the text
if isempty(img),
  out = block;
  return;
end;

% place block in img
[m,n] = size(block);
[M,N,P] = size(img);
inx1 = round([1:m] + (M-m)*pos(1));
inx2 = round([1:n] + (N-n)*pos(2));
if any(inx1 < 1) || any(inx1 > M),
  error('Text block out of range (vertical)');
end;
if any(inx2 < 1) || any(inx2 > N),
  error('Text block out of range (horizontal)');
end;

% set up out1, the text block in the image
if P == 3,
  % blend icolor with tcolor
  out1 = uint8(double(img(inx1,inx2,:))       .* repmat(reshape(icolor, [1 1 3]), [m n 1]) + ...
               double(repmat(block, [1 1 3])) .* repmat(reshape(tcolor, [1 1 3]), [m n 1]));
else,
  % set up output colors
  if length(tcolor) == 1,
    % text in two colors: icolor and tcolor
    if isempty(icolor), 
      outcolor = [tcolor tcolor]; % just have 2 points
    else,
      outcolor = [icolor tcolor];
    end;
  else,
    % text in (vector) tcolor
    outcolor = tcolor;
  end;
  % map range of the entries in block(:) thru outcolor
  inx = linspace(0, 255, length(outcolor));
  out1 = reshape(interp1(inx, outcolor, double(block(:)), 'nearest'), ...
                 size(block));
  % if icolor was given, ensure output=icolor where the bitmap == 0
  if ~isempty(icolor),
    out1(block == 0) = icolor;
  else,
    outB = img(inx1, inx2);
    out1(block == 0) = outB(block == 0);
  end;
end;
% plug it in
out = img;
out(inx1,inx2,:) = out1;
return;