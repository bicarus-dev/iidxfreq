@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1

if not defined IIDXFREQ_BUILD_IMAGE set "IIDXFREQ_BUILD_IMAGE=iidxfreq-build"
set "BUILD_TYPE=%~1"
if not defined BUILD_TYPE set "BUILD_TYPE=Release"

docker image inspect "%IIDXFREQ_BUILD_IMAGE%" >nul 2>&1
if errorlevel 1 (
    docker build --pull --tag "%IIDXFREQ_BUILD_IMAGE%" "%~dp0docker"
    if errorlevel 1 goto fail
)

docker run --rm ^
    --mount "type=bind,source=%CD%,target=/src" ^
    --mount "type=volume,source=iidxfreqbuild,target=/build" ^
    --workdir /src --env BUILD_ROOT=/build --env BUILD_TYPE --env TOOLCHAIN_64 ^
    "%IIDXFREQ_BUILD_IMAGE%" bash /src/build_all.sh
if errorlevel 1 goto fail
popd
exit /b 0

:fail
popd
exit /b 1