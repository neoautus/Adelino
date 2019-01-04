cat << EOF
=======================================================================
This script downloads and extracts the LUFA library needed by Adelino.

LUFA is the Lightweight USB Framework for AVRs, written by Dean Camera.
You can find it in: https://github.com/abcminiuser/lufa
Official site: http://www.lufa-lib.org/
=======================================================================
EOF
wget https://github.com/abcminiuser/lufa/archive/LUFA-111009.tar.gz
# Remove old LUFA directory
if [ -e lufa-LUFA-111009 ]; then
  rm -rf lufa-LUFA-111009
fi
tar xf LUFA-111009.tar.gz
# Preserve the source tar inside LUFA directory
mv LUFA-111009.tar.gz lufa-LUFA-111009
echo --------------------------------------
echo LUFA library downloaded and extracted.
echo --------------------------------------
