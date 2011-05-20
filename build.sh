#!/bin/sh

# This is a the master build script - it is intended to be run by parabuild
# It is called by a wrapper script in the shared repository which sets up
# the environment from the various BuildParams files and does all the build 
# result post-processing.
#
# PLEASE NOTE:
#
# * This script is interpreted on three platforms, including windows and cygwin
#   Cygwin can be tricky....
# * The special style in which python is invoked is intentional to permit
#   use of a native python install on windows - which requires paths in DOS form
# * This script relies heavily on parameters defined in BuildParams
# * The basic convention is that the build name can be mapped onto a mercurial URL,
#   which is also used as the "branch" name.

build_dir_Darwin()
{
  echo build-darwin-i386
}

build_dir_Linux()
{
  echo build-linux-i686
}

build_dir_CYGWIN()
{
  echo build-vc100
}

installer_Darwin()
{
  ls -1td "$(build_dir_Darwin ${last_built_variant:-Release})/newview/"*.dmg 2>/dev/null | sed 1q
}

installer_Linux()
{
  ls -1td "$(build_dir_Linux ${last_built_variant:-Release})/newview/"*.tar.bz2 2>/dev/null | sed 1q
}

installer_CYGWIN()
{
  v=${last_built_variant:-Release}
  d=$(build_dir_CYGWIN $v)
  if [ -r "$d/newview/$v/touched.bat" ]
  then
    p=$(sed 's:.*=::' "$d/newview/$v/touched.bat")
    echo "$d/newview/$v/$p"
  fi
}

pre_build()
{
  local variant="$1"
  begin_section "Pre$variant"
    [ -n "$master_message_template_checkout" ] \
    && [ -r "$master_message_template_checkout/message_template.msg" ] \
    && template_verifier_master_url="-DTEMPLATE_VERIFIER_MASTER_URL=file://$master_message_template_checkout/message_template.msg"

    "$AUTOBUILD" configure -c $variant -- \
     -DPACKAGE:BOOL=ON \
     -DRELEASE_CRASH_REPORTING:BOOL=ON \
     -DVIEWER_CHANNEL:STRING="\"$viewer_channel\"" \
     -DVIEWER_LOGIN_CHANNEL:STRING="\"$viewer_login_channel\"" \
     -DGRID:STRING="\"$viewer_grid\"" \
     -DLL_TESTS:BOOL="$run_tests" \
     -DTEMPLATE_VERIFIER_OPTIONS:STRING="$template_verifier_options" $template_verifier_master_url
 end_section "Pre$variant"
}

build()
{
  local variant="$1"
  if $build_viewer
  then
    begin_section "Viewer$variant"
    if "$AUTOBUILD" build --no-configure -c $variant
    then
      echo true >"$build_dir"/build_ok
    else
      echo false >"$build_dir"/build_ok
    fi
    end_section "Viewer$variant"
  fi
}

# This is called from the branch independent script upon completion of all platform builds.
build_docs()
{
  begin_section Docs
  # Stub code to generate docs
  echo Hello world  > documentation.txt
  upload_item docs documentation.txt text/plain
  end_section Docs
}


# Check to see if we were invoked from the wrapper, if not, re-exec ourselves from there
if [ "x$arch" = x ]
then
  top=`hg root`
  if [ -x "$top/../buildscripts/hg/bin/build.sh" ]
  then
    exec "$top/../buildscripts/hg/bin/build.sh" "$top"
  else
    cat <<EOF
This script, if called in a development environment, requires that the branch
independent build script repository be checked out next to this repository.
This repository is located at http://hg.lindenlab.com/parabuild/buildscripts
EOF
    exit 1
  fi
fi

# Check to see if we're skipping the platform
eval '$build_'"$arch" || pass

# Run the version number update script
# File no longer exists in code-sep branch, so let's make sure it exists in order to use it.
if test -f scripts/update_version_files.py ; then
  begin_section UpdateVer
  eval $(python scripts/update_version_files.py \
                --channel="$viewer_channel" \
                --server_channel="$server_channel" \
                --revision=$revision \
                --verbose \
         | sed -n -e "s,Setting viewer channel/version: '\([^']*\)' / '\([^']*\)',VIEWER_CHANNEL='\1';VIEWER_VERSION='\2',p")\
  || fail update_version_files.py
  echo "{\"Type\":\"viewer\",\"Version\":\"${VIEWER_VERSION}\"}" > summary.json
  end_section UpdateVer
fi

# Now retrieve the version for use in the version manager
# First three parts only, $revision will be appended automatically.
build_viewer_update_version_manager_version=`python scripts/get_version.py --viewer-version | sed 's/\.[0-9]*$//'`

if [ -z "$AUTOBUILD" ]
then
  export autobuild_dir="$here/../../../autobuild/bin/"
  if [ -d "$autobuild_dir" ]
  then
    export AUTOBUILD="$autobuild_dir"autobuild
    if [ -x "$AUTOBUILD" ]
    then
      # *HACK - bash doesn't know how to pass real pathnames to native windows python
      case "$arch" in
      CYGWIN) AUTOBUILD=$(cygpath -u $AUTOBUILD.cmd) ;;
      esac
    else
      record_failure "Not executable: $AUTOBUILD"
      exit 1
    fi
  else
    record_failure "Not found: $autobuild_dir"
    exit 1
  fi
fi

# load autbuild provided shell functions and variables
# Merov: going back to the previous code that passes even if it fails catching a failure
# TODO: use the correct code here under and fix the llbase import in python code
#if "$AUTOBUILD" source_environment > source_environment
#then
#  . source_environment
#else
  # dump environment variables for debugging
#  env|sort
#  record_failure "autobuild source_environment failed"
#  cat source_environment >&3
#  exit 1
#fi
eval "$("$AUTOBUILD" source_environment)"

# dump environment variables for debugging
env|sort


# Install packages.
"$AUTOBUILD" install --skip-license-check

# Now run the build
succeeded=true
build_processes=
last_built_variant=
for variant in $variants
do
  eval '$build_'"$variant" || continue
  eval '$build_'"$arch"_"$variant" || continue

  # Only the last built arch is available for upload
  last_built_variant="$variant"

  begin_section "Do$variant"
  build_dir=`build_dir_$arch $variant`
  build_dir_stubs="$build_dir/win_setup/$variant"
  rm -rf "$build_dir"
  mkdir -p "$build_dir"
  mkdir -p "$build_dir/tmp"
  #export TMP="$build_dir/tmp"
  if pre_build "$variant" "$build_dir" >> "$build_log" 2>&1
  then
    if $build_link_parallel
    then
      begin_section BuildParallel
      ( build "$variant" "$build_dir" > "$build_dir/build.log" 2>&1 ) &
      build_processes="$build_processes $!"
      end_section BuildParallel
    else
      begin_section "Build$variant"
      build "$variant" "$build_dir" 2>&1 | tee -a "$build_log" | grep --line-buffered "^##teamcity"
      if `cat "$build_dir/build_ok"`
      then
        echo so far so good.
      else
        record_failure "Build of \"$variant\" failed."
      fi
      end_section "Build$variant"
    fi
  else
    record_failure "Build Prep for \"$variant\" failed."
  fi
  end_section "Do$variant"
done

# If we are building variants in parallel, wait, then collect results.
# This requires that the build dirs are variant specific
if $build_link_parallel && [ x"$build_processes" != x ]
then
  begin_section WaitParallel
  wait $build_processes
  for variant in $variants
  do
    eval '$build_'"$variant" || continue
    eval '$build_'"$arch"_"$variant" || continue

    begin_section "Build$variant"
    build_dir=`build_dir_$arch $variant`
    build_dir_stubs="$build_dir/win_setup/$variant"
    tee -a $build_log < "$build_dir/build.log" | grep --line-buffered "^##teamcity"
    if `cat "$build_dir/build_ok"`
    then
      echo so far so good.
    else
      record_failure "Parallel build of \"$variant\" failed."
    fi
    end_section "Build$variant"
  done
  end_section WaitParallel
fi

# check status and upload results to S3
if $succeeded
then
  if $build_viewer
  then
    begin_section Upload
    # Upload installer - note that ONLY THE FIRST ITEM uploaded as "installer"
    # will appear in the version manager.
    package=$(installer_$arch)
    if [ x"$package" = x ] || test -d "$package"
    then
      # Coverity doesn't package, so it's ok, anything else is fail
      succeeded=$build_coverity
    else
      upload_item installer "$package" binary/octet-stream
      upload_item quicklink "$package" binary/octet-stream
      [ -f summary.json ] && upload_item installer summary.json text/plain

      # Upload crash reporter files.
      case "$last_built_variant" in
      Release)
        for symbolfile in $symbolfiles
        do
          upload_item symbolfile "$build_dir/$symbolfile" binary/octet-stream
        done
        ;;
      esac

      # Upload stub installers
      upload_stub_installers "$build_dir_stubs"
    fi
    end_section Upload
  else
    echo skipping viewer
  fi
else
  echo skipping upload of build results due to failed build.
fi

# The branch independent build.sh script invoking this script will finish processing
$succeeded || exit 1
