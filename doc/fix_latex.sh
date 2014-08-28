for i in `grep -lr -e"figure\}\[H\]" *`; do sed -i -e 's,figure\}\[H\],figure\}\[h\],g' $i; done
