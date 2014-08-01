OUT=`basename $1`
ARCHES="x86 x86_64"

cat <<EOF
/* Architecture dependant wrapper, included by non-architecture   *
 * dependant header files. This file was generated automatically. *
 * Do not edit it directly.
 */

#include <sea/config.h>

#if CONFIG_ARCH == 0
  #error "unknown architecture"
EOF


for i in $ARCHES; do
	ARCHNAME=$(echo $i | tr '[:lower:]' '[:upper:]')
	NAME=""
	if (echo $OUT | grep "-" > /dev/null); then
		NAME=`echo $OUT | sed -r "s/([a-zA-Z0-9]+)\-([a-zA-Z0-9\-]+)\.h/\1\/\2-$i\.h/"`
	else
		NAME=`echo $OUT | sed -r "s/([a-zA-Z0-9]+)\.h/\1-$i\.h/"`
	fi
	echo "#elif CONFIG_ARCH == TYPE_ARCH_$ARCHNAME"
	echo "  #include <sea/$NAME>"

done

echo "#endif"

