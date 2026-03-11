
IPKG_TOP_DIR=.
NFS_SHARE_DIR=/home/eeb001/loping_bak/git_pull_eeb001_/share

find ${IPKG_TOP_DIR}/../elegoo/build -name "*.so*" | xargs -I {} cp {} ${NFS_SHARE_DIR} -rf
cp ${IPKG_TOP_DIR}/../elegoo/build/elegoo_printer ${NFS_SHARE_DIR} -rf
cp ${IPKG_TOP_DIR}/../elegoo/build/debug_window ${NFS_SHARE_DIR} -rf
cd ${NFS_SHARE_DIR}
chmod +x ./elegoo_printer
chmod +x ./debug_window
cd -
cp ${IPKG_TOP_DIR}/../debugfile ${NFS_SHARE_DIR} -rf
