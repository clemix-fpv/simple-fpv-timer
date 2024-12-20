# SPDX-License-Identifier: GPL-3.0+

Import('env')

import os
import gzip
import shutil
from pathlib import Path
import glob
import subprocess

def prepare_www_files(source, target, env):

    filetypes_to_gzip = ['js', 'html', 'css']

    proj_dir = Path(env.get("PROJECT_DIR"))
    data_src_dir = os.path.join(proj_dir, 'data_src')
    tmp_dir = os.path.join(proj_dir, 'data_tmp')
    src_dir = os.path.join(proj_dir, 'src')
    dst_header_file = os.path.join(src_dir, "static_files.h")

    if(os.path.exists(tmp_dir)):
        print('  Delete temproary dir {}'.format(tmp_dir))
        shutil.rmtree(tmp_dir)

    print('  Re-creating empty temporary dir {} '.format(tmp_dir))
    os.mkdir(tmp_dir)

    files_to_gzip = []
    for extension in filetypes_to_gzip:
        files_to_gzip.extend(glob.glob(os.path.join(data_src_dir, '*.' + extension)))
    
    print('  files to gzip: ' + str(files_to_gzip))

    all_files = glob.glob(os.path.join(data_src_dir, '*.*'))
    files_to_copy = list(set(all_files) - set(files_to_gzip))

    print('  files to copy: ' + str(files_to_copy))

    dst_files = []
    for file in files_to_copy:
        print('  COPY: ' + file)
        dst = os.path.join(tmp_dir, os.path.basename(file))
        shutil.copy(file, dst)
        dst_files.append(dst)

    
    for file in files_to_gzip:
        dst = os.path.join(tmp_dir, os.path.basename(file) + '.gz')
        dst_files.append(dst)
        print('  GZIP: ' + file + ' -> ' + dst)
        cmd = 'gzip -9 < {S} > {D}'.format(S=file, D=dst)
        subprocess.check_call(cmd, shell=True)

    fcnt = 0;
    if os.path.exists(dst_header_file):
        print('  Delete existing destination: ' + dst_header_file)
        os.remove(dst_header_file)
    with open(dst_header_file, 'a') as fdst:
        print('  CREATE: ' + dst_header_file)
        fdst.write('#include <stdio.h>\n')

        h_file_content = []
        for file in dst_files:
            cnt = 0
            filename = os.path.basename(file)
            with open(file, 'rb') as fsrc:
                print('   PROCESS: ' + filename)
                fdst.write("/* %s */\nconst unsigned char file_%02d[] = {" % (filename, fcnt))
                while 1:
                    byte = fsrc.read(1)
                    if not byte:
                        break
                    if cnt > 0:
                        fdst.write(", ")
                    if cnt % 8 == 0:
                        fdst.write("\n    ")
                    cnt+=1
                    fdst.write("0x%02x" % ord(byte))
                fdst.write("\n};\n\n")

            gzip = 0
            filetype ="text/html"
            if filename.endswith('.gz'):
                filename = filename[0:-3]
                gzip = 1
            if filename.endswith('.css'):
                filetype = "text/css"
            if filename.endswith('.js'):
                filetype = "text/javascript"
            if filename.endswith('.ogg'):
                filetype = "audio/ogg"
            if filename.endswith('.svg'):
                filetype = "image/svg+xml"
            h_file_content.append((filename, gzip, filetype, "file_%02d" %fcnt, cnt))
            fcnt+=1

        fdst.write("""
struct static_files {
    const char *name;
    unsigned int is_gzip;
    const char *type;
    const unsigned char *data;
    size_t data_len;
}; 

const struct static_files STATIC_FILES[] = {
""")
        
        for hc in h_file_content:
            fdst.write('    {.name = %-30s .is_gzip = %d, .type = %-18s .data = %-7s, .data_len = %-5d },\n' %('"/' + hc[0]+'",', hc[1], '"'+hc[2]+'",', hc[3], hc[4]))
        fdst.write("    {.name = NULL, .data = NULL}\n};\n")

    # Cleanup
    if(os.path.exists(tmp_dir)):
        print('  Delete temproary dir {}'.format(tmp_dir))
        shutil.rmtree(tmp_dir)



env.AddPreAction("${BUILD_DIR}/src/gui.c.o", prepare_www_files)
#env.AddPreAction("program", prepare_www_files)
