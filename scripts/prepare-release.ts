const newVersion = Bun.argv[2];

if (!newVersion || !/^\d+\.\d+\.\d+$/.test(newVersion)) {
    console.error("Error: Version in MAJOR.MINOR.PATCH format required.");
    process.exit(1);
}

const [major, minor, patch] = newVersion.split(".");

async function updateVersion() {
    // 1. Update app_state.h
    const appStateFile = Bun.file("src/app_state.h");
    let appState = await appStateFile.text();

    appState = appState.replace(/APP_VERSION_MAJOR = \d+/, `APP_VERSION_MAJOR = ${major}`);
    appState = appState.replace(/APP_VERSION_MINOR = \d+/, `APP_VERSION_MINOR = ${minor}`);
    appState = appState.replace(/APP_VERSION_PATCH = \d+/, `APP_VERSION_PATCH = ${patch}`);

    await Bun.write("src/app_state.h", appState);
    console.log(`✅ Updated app_state.h to v${newVersion}`);

    // 2. Update resource.rc
    const rcFile = Bun.file("resource.rc");
    let rcContent = await rcFile.text();

    rcContent = rcContent.replace(/(FILEVERSION |PRODUCTVERSION )\d+,\d+,\d+,\d+/g, `$1${major},${minor},${patch},0`);
    rcContent = rcContent.replace(/VALUE "(FileVersion|ProductVersion)", "[^"]+"/g, `VALUE "$1", "${newVersion}.0"`);

    await Bun.write("resource.rc", rcContent);
    console.log(`✅ Updated resource.rc to v${newVersion}`);
}

await updateVersion();