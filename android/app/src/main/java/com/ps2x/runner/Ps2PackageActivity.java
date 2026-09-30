package com.ps2x.runner;

import android.app.NativeActivity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.util.Arrays;

/** Stages the packaged game tree before NativeActivity starts its native library. */
public final class Ps2PackageActivity extends NativeActivity {
    private static final String TAG = "Ps2PackageActivity";
    private static final String GAME_ASSET_ROOT = "game";
    private static final String BOOT_ELF_RELATIVE_PATH = "boot.elf";
    private static final String PACKAGE_MARKER_ASSET = "ps2-package.marker";
    private static final String PACKAGE_MARKER_FILE = ".ps2-package-marker";
    private static final int MAX_MARKER_BYTES = 4096;
    private static final int COPY_BUFFER_BYTES = 64 * 1024;

    /** Intent contract for the native runner; paths point into app-private storage. */
    public static final String EXTRA_GAME_ROOT_PATH =
            "com.ps2x.runner.extra.GAME_ROOT_PATH";
    public static final String EXTRA_BOOT_ELF_PATH =
            "com.ps2x.runner.extra.BOOT_ELF_PATH";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        final File gameRoot;
        final File bootElf;
        try {
            File filesRoot = getFilesDir().getCanonicalFile();
            gameRoot = new File(filesRoot, GAME_ASSET_ROOT).getAbsoluteFile();
            if (Files.isSymbolicLink(gameRoot.toPath())
                    || !gameRoot.getCanonicalFile().equals(gameRoot)) {
                throw new IOException("The app-private game directory is not a safe path");
            }
            bootElf = new File(gameRoot, BOOT_ELF_RELATIVE_PATH).getAbsoluteFile();

            installPackagedGame(getAssets(), filesRoot, gameRoot);

            getIntent().putExtra(EXTRA_GAME_ROOT_PATH, gameRoot.getAbsolutePath());
            getIntent().putExtra(EXTRA_BOOT_ELF_PATH, bootElf.getAbsolutePath());
        } catch (IOException | SecurityException | IllegalArgumentException e) {
            String detail = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage();
            String message = "Game package could not be loaded: " + detail;
            Log.e(TAG, message, e);
            Toast.makeText(this, message, Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        // NativeActivity may load the game library during this call, so assets are staged first.
        super.onCreate(savedInstanceState);
    }

    private void installPackagedGame(AssetManager assets, File filesRoot, File gameRoot)
            throws IOException {
        verifyPackagedBootElf(assets);

        File markerFile = new File(filesRoot, PACKAGE_MARKER_FILE);
        byte[] packagedMarker = readPackagedMarker(assets);
        if (packagedMarker != null
                && Arrays.equals(packagedMarker, readStoredMarker(markerFile))
                && isElfFile(new File(gameRoot, BOOT_ELF_RELATIVE_PATH))) {
            Log.i(TAG, "Packaged game marker matches; keeping staged files.");
            return;
        }

        File stagingRoot = new File(filesRoot, ".game.staging").getAbsoluteFile();
        File backupRoot = new File(filesRoot, ".game.previous").getAbsoluteFile();

        // Recover the previous tree if the process stopped between the two directory renames.
        if (Files.isSymbolicLink(backupRoot.toPath())) {
            deleteRecursively(backupRoot);
        }
        if (!existsWithoutFollowingLinks(gameRoot) && existsWithoutFollowingLinks(backupRoot)) {
            if (!backupRoot.renameTo(gameRoot)) {
                throw new IOException("Could not restore the previous game package");
            }
        }
        deleteRecursively(stagingRoot);
        deleteRecursively(backupRoot);
        ensureDirectory(stagingRoot);

        copyAssetNode(assets, GAME_ASSET_ROOT, "", stagingRoot);
        File stagedBootElf = checkedChild(stagingRoot, BOOT_ELF_RELATIVE_PATH);
        if (!isElfFile(stagedBootElf)) {
            throw new IOException("Packaged assets do not contain a valid game/boot.elf");
        }

        boolean movedOldTree = false;
        if (existsWithoutFollowingLinks(gameRoot)) {
            if (!gameRoot.renameTo(backupRoot)) {
                throw new IOException("Could not move the existing game package");
            }
            movedOldTree = true;
        }

        if (!stagingRoot.renameTo(gameRoot)) {
            if (movedOldTree && !backupRoot.renameTo(gameRoot)) {
                Log.e(TAG, "Could not restore the existing game package after install failure");
            }
            throw new IOException("Could not activate the staged game package");
        }

        deleteRecursively(backupRoot);
        if (packagedMarker == null) {
            deleteRecursively(markerFile);
        } else {
            writeStoredMarker(markerFile, packagedMarker);
        }
    }

    private static void verifyPackagedBootElf(AssetManager assets) throws IOException {
        try (InputStream input = assets.open(GAME_ASSET_ROOT + "/" + BOOT_ELF_RELATIVE_PATH)) {
            if (!hasElfMagic(input)) {
                throw new IOException("Packaged assets are missing a valid assets/game/boot.elf");
            }
        } catch (FileNotFoundException e) {
            throw new IOException("Packaged assets are missing assets/game/boot.elf", e);
        }
    }

    private static byte[] readPackagedMarker(AssetManager assets) throws IOException {
        try (InputStream input = assets.open(PACKAGE_MARKER_ASSET)) {
            byte[] marker = readBounded(input, MAX_MARKER_BYTES);
            return marker.length == 0 ? null : marker;
        } catch (FileNotFoundException e) {
            // Without a marker the package is copied on each launch, avoiding stale cache hits.
            return null;
        }
    }

    private static byte[] readStoredMarker(File markerFile) throws IOException {
        if (!existsWithoutFollowingLinks(markerFile)
                || Files.isSymbolicLink(markerFile.toPath()) || !markerFile.isFile()) {
            return null;
        }
        try (InputStream input = new FileInputStream(markerFile)) {
            try {
                return readBounded(input, MAX_MARKER_BYTES);
            } catch (IOException tooLargeOrUnreadable) {
                return null;
            }
        }
    }

    private static byte[] readBounded(InputStream input, int limit) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream(Math.min(limit, 256));
        byte[] buffer = new byte[1024];
        int total = 0;
        int count;
        while ((count = input.read(buffer)) != -1) {
            if (count > limit - total) {
                throw new IOException("Package marker exceeds " + limit + " bytes");
            }
            output.write(buffer, 0, count);
            total += count;
        }
        return output.toByteArray();
    }

    private static void writeStoredMarker(File markerFile, byte[] marker) throws IOException {
        File temporary = new File(markerFile.getParentFile(), markerFile.getName() + ".tmp");
        deleteRecursively(temporary);
        try (FileOutputStream output = new FileOutputStream(temporary)) {
            output.write(marker);
            output.getFD().sync();
        }
        if (existsWithoutFollowingLinks(markerFile) && !markerFile.delete()) {
            deleteRecursively(temporary);
            throw new IOException("Could not replace the installed package marker");
        }
        if (!temporary.renameTo(markerFile)) {
            deleteRecursively(temporary);
            throw new IOException("Could not save the installed package marker");
        }
    }

    private static void copyAssetNode(AssetManager assets, String assetPath, String relativePath,
                                      File stagingRoot) throws IOException {
        File destination = checkedChild(stagingRoot, relativePath);
        String[] children = assets.list(assetPath);
        if (children.length > 0) {
            ensureDirectory(destination);
            for (String child : children) {
                validateAssetComponent(child);
                String childAssetPath = assetPath + "/" + child;
                String childRelativePath = relativePath.isEmpty()
                        ? child : relativePath + "/" + child;
                copyAssetNode(assets, childAssetPath, childRelativePath, stagingRoot);
            }
            return;
        }

        InputStream assetInput;
        try {
            assetInput = assets.open(assetPath);
        } catch (FileNotFoundException e) {
            // AssetManager.list() is empty for both empty directories and files.
            ensureDirectory(destination);
            return;
        }

        File parent = destination.getParentFile();
        if (parent == null) {
            throw new IOException("Invalid destination for packaged asset " + assetPath);
        }
        ensureDirectory(parent);
        try (InputStream input = assetInput;
             FileOutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[COPY_BUFFER_BYTES];
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
        }
    }

    private static void validateAssetComponent(String component) throws IOException {
        if (component == null || component.isEmpty() || ".".equals(component)
                || "..".equals(component) || component.indexOf('/') >= 0
                || component.indexOf('\\') >= 0 || component.indexOf('\0') >= 0) {
            throw new IOException("Invalid packaged asset path component");
        }
    }

    private static File checkedChild(File root, String relativePath) throws IOException {
        File canonicalRoot = root.getCanonicalFile();
        File child = relativePath.isEmpty() ? canonicalRoot : new File(canonicalRoot, relativePath);
        File canonicalChild = child.getCanonicalFile();
        String rootPath = canonicalRoot.getPath();
        String childPath = canonicalChild.getPath();
        if (!childPath.equals(rootPath)
                && !childPath.startsWith(rootPath + File.separator)) {
            throw new IOException("Packaged asset path escapes its destination");
        }
        return canonicalChild;
    }

    private static boolean hasElfMagic(InputStream input) throws IOException {
        return input.read() == 0x7f && input.read() == 'E'
                && input.read() == 'L' && input.read() == 'F';
    }

    private static boolean isElfFile(File file) throws IOException {
        if (!existsWithoutFollowingLinks(file) || Files.isSymbolicLink(file.toPath())
                || !file.isFile() || file.length() < 4) {
            return false;
        }
        try (InputStream input = new FileInputStream(file)) {
            return hasElfMagic(input);
        }
    }

    private static void ensureDirectory(File directory) throws IOException {
        if (existsWithoutFollowingLinks(directory)) {
            if (!directory.isDirectory()) {
                throw new IOException("Expected a directory at " + directory.getAbsolutePath());
            }
            return;
        }
        if (!directory.mkdirs() && !directory.isDirectory()) {
            throw new IOException("Could not create directory " + directory.getAbsolutePath());
        }
    }

    private static boolean existsWithoutFollowingLinks(File path) {
        return Files.exists(path.toPath(), LinkOption.NOFOLLOW_LINKS);
    }

    private static void deleteRecursively(File path) throws IOException {
        if (!existsWithoutFollowingLinks(path)) {
            return;
        }
        if (Files.isSymbolicLink(path.toPath())) {
            Files.delete(path.toPath());
            return;
        }
        if (path.isDirectory()) {
            File[] children = path.listFiles();
            if (children == null) {
                throw new IOException("Could not list directory " + path.getAbsolutePath());
            }
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        Files.delete(path.toPath());
    }
}
