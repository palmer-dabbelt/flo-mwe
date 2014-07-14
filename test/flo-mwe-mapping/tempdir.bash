set -ex

tempdir=`mktemp -d -t ptest-flo-mwe.XXXXXXXXXX`
trap "rm -rf $tempdir" EXIT
cd $tempdir
