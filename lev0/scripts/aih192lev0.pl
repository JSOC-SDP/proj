#!/usr/bin/perl -w
$to_list = join ",", 'jps@lmsal.com', 'boerner@lmsal.com', 'green@lmsal.com',
        'wolfson@lmsal.com', 'zoe@lmsal.com', 'jeneen@sun.stanford.edu',
        'rock@sun.stanford.edu', 'thailand@sun.stanford.edu',
        'couvidat@stanford.edu',
        '6509965043@txt.att.net',
        '6504503716@txt.att.net',
        '6502248075@txt.att.net',
        '4084317110@txt.att.net',
        '5103252489@vtext.com',
        '5178969022@vtext.com',
        '6503878335@txt.att.net',
        '6507436500@tmomail.net',
        '6509968590@txt.att.net';
$msg_file = "$ENV{HOME}/bit_flip_his.txt";
exit if -e $msg_file;
if ($t = shift @ARGV) { $threshold = $t; } else { $threshold = 5; }
$cmd = "/home/jsoc/cvs/Development/JSOC/bin/linux_x86_64/show_info";
for ($cam=1; $cam<5; $cam++) {
  $fsn0 = `$cmd -q key=fsn 'aia.lev0[:#\$]'` - 3999;
  $n = `$cmd -c 'aia.lev0[$fsn0/4000][?aihis192>9?][?camera=$cam?]'`;
  @w = split /\s+/, $n;
  if ($w[0] > $threshold) {
    $subj = "AIA camera $cam histogram anomaly";
    open MSG, ">$msg_file" or die "Can not write mail";
    print MSG "\n", `date -u`,`date`, "$subj detected.\n";
    close MSG or die "Can not close mail message file";
    `mail -s "$subj" $to_list < $msg_file`;
  }
}