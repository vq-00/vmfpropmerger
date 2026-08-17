#include "vmfpropmerger/options.h"
#include "vmfpropmerger/vmf.h"
#include "vmfpropmerger/smd.h"
#include "vmfpropmerger/vpk.h"
#include "vmfpropmerger/transform.h"

using namespace vmfpropmerger;

int main(int argc, char** argv) {
    Options options;
    if (!parseOptions(argc, argv, options)) return 1;
    if (options.showVersion) {
        std::cout << "Reactive Drop VMF Prop Merger 3.0\n";
        return 0;
    }
    if (options.showHelp) {
        printBanner();
        printUsage();
        return 0;
    }
    printBanner();

    options.vmfPath = fs::absolute(options.vmfPath);
    options.gameRoot = fs::absolute(options.gameRoot);

    if (options.modelName.empty()) {
        options.modelName = options.vmfPath.stem().string() + "_merged.mdl";
    }
    if (options.targetName.empty()) {
        options.targetName = options.vmfPath.stem().string() + "_merged_props";
    }
    if (options.outputDirectory.empty()) {
        options.outputDirectory = options.gameRoot / "models" / "merged_props";
    } else if (!options.outputDirectory.is_absolute()) {
        options.outputDirectory = options.gameRoot / options.outputDirectory;
    }

    const fs::path outputModelPath = options.outputDirectory / options.modelName;
    fs::path outputVMF;
    if (options.outputVmfExplicit) {
        outputVMF = options.outputVmf;
        if (!outputVMF.is_absolute()) outputVMF = options.vmfPath.parent_path() / outputVMF;
    } else {
        outputVMF = options.vmfPath.parent_path() / (options.vmfPath.stem().string() + "_merged.vmf");
    }

    std::error_code ec;
    if (!fs::exists(options.vmfPath, ec)) {
        std::cerr << "VMF file does not exist:\n" << options.vmfPath << "\n";
        return 1;
    }
    if (!fs::is_regular_file(options.vmfPath, ec)) {
        std::cerr << "VMF path is not a file:\n" << options.vmfPath << "\n";
        return 1;
    }
    if (!fs::exists(options.gameRoot, ec)) {
        std::cerr << "Game directory does not exist:\n" << options.gameRoot << "\n";
        return 1;
    }

    CoordinateMap coordinateMap;
    if (!parseCoordinateMap(options.coordMap, coordinateMap)) {
        std::cerr << "Invalid coordinate map: " << options.coordMap << "\n\n"
                  << "Valid components are:\n x y z -x -y -z\nExample:\n --coord-map -y,x,z\n";
        return 1;
    }

    loadExcludeFile(options.excludeFile, options.excludeModels);

    if (!options.quiet) {
        std::cout << "VMF:\n " << options.vmfPath << "\n\n"
                  << "Game root:\n " << options.gameRoot << "\n\n"
                  << "Output model:\n " << outputModelPath << "\n\n"
                  << "Output VMF:\n " << outputVMF << "\n\n"
                  << "Coordinate map:\n " << options.coordMap << "\n"
                  << "Rotation:\n " << (options.applyRotation ? "enabled" : "disabled") << "\n"
                  << "Normal transform:\n " << (options.transformNormals ? "enabled" : "disabled") << "\n"
                  << "Global scale:\n " << options.globalScale << "\n"
                  << "Global offset:\n " << options.globalOffset.x << " " << options.globalOffset.y << " " << options.globalOffset.z << "\n"
                  << "Triangles/body:\n " << options.trianglesPerBody << "\n\n";
    }

    std::vector<VMFObject> roots;
    if (!options.quiet) std::cout << "Parsing VMF...\n";
    if (!parseVMF(options.vmfPath, roots)) {
        std::cerr << "Failed to parse VMF.\n";
        return 1;
    }

    std::vector<PropDynamic> props = collectProps(roots);
    std::map<std::string, const VMFObject*> namedEntities;
    collectNamedEntities(roots, namedEntities);

    for (PropDynamic& prop : props) {
        std::string parentName;
        prop.hasParent = getKey(prop.object, "parentname", parentName) && !trim(parentName).empty();
        prop.parentName = trim(parentName);
        prop.worldOrigin = prop.origin;
        prop.worldRotation = sourceAngleMatrix(prop.angles);
        if (prop.hasParent) {
            std::set<const VMFObject*> resolving;
            if (!resolveEntityWorldTransform(prop.object, namedEntities, prop.worldOrigin, prop.worldRotation, resolving)) {
                prop.worldOrigin = prop.origin;
                prop.worldRotation = sourceAngleMatrix(prop.angles);
            }
        }
    }

    if (!options.quiet) std::cout << "Found " << props.size() << " prop_dynamic entities.\n";
    if (props.empty()) { std::cout << "Nothing to merge.\n"; return 0; }

    std::vector<PropDynamic> selectedProps;
    int excludedCount = 0;
    for (const PropDynamic& prop : props) {
        if (shouldProcessProp(prop, options)) {
            selectedProps.push_back(prop);
        } else {
            ++excludedCount;
            if (options.verbose) std::cout << "EXCLUDED: " << prop.model << "\n";
        }
    }
    if (!options.quiet) {
        std::cout << "Selected " << selectedProps.size() << " prop_dynamic entities.\n"
                  << "Excluded " << excludedCount << " prop_dynamic entities.\n\n";
    }
    if (selectedProps.empty()) { std::cout << "No props remain after filtering.\n"; return 0; }
    if (options.dryRun) {
        std::cout << "\nDry run complete.\nNo files were modified and no external tools were run.\n";
        return 0;
    }

    fs::path crowbar;
    if (options.crowbarExplicit) crowbar = fs::absolute(options.crowbar);
    else crowbar = findCrowbar(options.gameRoot);
    fs::path studiomdl;
    if (options.studiomdlExplicit) studiomdl = fs::absolute(options.studiomdl);
    else studiomdl = findStudioMDL(options.gameRoot);

    if (crowbar.empty()) {
        std::cerr << "Crowbar command-line decompiler was not found.\n\n"
                  << "Use:\n --crowbar <path>\n\nor set CROWBAR_EXE.\n";
        return 1;
    }
    if (studiomdl.empty()) {
        std::cerr << "studiomdl.exe was not found.\n\nUse:\n --studiomdl <path>\n";
        return 1;
    }
    if (!options.quiet) {
        std::cout << "Crowbar:\n " << crowbar << "\n\n"
                  << "StudioMDL:\n " << studiomdl << "\n\n";
    }

    if (!options.workDirExplicit) {
        options.workDir = options.vmfPath.parent_path() / (options.vmfPath.stem().string() + "_propmerge");
    } else if (!options.workDir.is_absolute()) {
        options.workDir = options.vmfPath.parent_path() / options.workDir;
    }
    if (!options.keepWork) {
        std::error_code workEc;
        fs::remove_all(options.workDir, workEc);
        if (workEc) {
            std::cerr << "Could not clean work directory:\n" << options.workDir << "\n" << workEc.message() << "\n";
            return 1;
        }
    }
    fs::create_directories(options.workDir / "models", ec);
    fs::create_directories(options.workDir / "decompiled", ec);
    if (ec) {
        std::cerr << "Could not create work directory:\n" << options.workDir << "\n";
        return 1;
    }

    VpkDatabase database;
    database.initialize(options.gameRoot, !options.noVpk, options.verbose);

    std::vector<SMDMesh> mergedMeshes;
    SMDMesh mergedCollisionMesh;
    std::vector<std::string> allMaterials;
    std::map<std::string, CachedModel> modelCache;
    std::unordered_set<uint64_t> successfullyMergedEntityIds;
    int successCount = 0, failureCount = 0;

    size_t decompileCounter = 0;
    for (size_t i = 0; i < selectedProps.size(); ++i) {
        PropDynamic& prop = selectedProps[i];
        std::cout << "\n==================================================\n"
                  << "Processing prop " << (i+1) << " / " << selectedProps.size() << "\n"
                  << "Model: " << prop.model << "\n"
                  << "Origin: " << prop.origin.x << " " << prop.origin.y << " " << prop.origin.z << "\n"
                  << "Angles: " << prop.angles.x << " " << prop.angles.y << " " << prop.angles.z << "\n"
                  << "World Origin: " << prop.worldOrigin.x << " " << prop.worldOrigin.y << " " << prop.worldOrigin.z << "\n"
                  << "Parent: " << (prop.hasParent ? prop.parentName : "<none>") << "\n"
                  << "World Rotation Matrix:\n"
                  << " " << prop.worldRotation.m[0][0] << " " << prop.worldRotation.m[0][1] << " " << prop.worldRotation.m[0][2] << "\n"
                  << " " << prop.worldRotation.m[1][0] << " " << prop.worldRotation.m[1][1] << " " << prop.worldRotation.m[1][2] << "\n"
                  << " " << prop.worldRotation.m[2][0] << " " << prop.worldRotation.m[2][1] << " " << prop.worldRotation.m[2][2] << "\n"
                  << "Scale: " << prop.modelScale << "\n"
                  << "Scale Axis: " << prop.modelScaleAxis.x << " " << prop.modelScaleAxis.y << " " << prop.modelScaleAxis.z << "\n"
                  << "==================================================\n";

        const std::string cacheKey = lowerString(normalizeSlashes(removeLeadingSlash(prop.model)));
        auto cached = modelCache.find(cacheKey);

        if (cached == modelCache.end()) {
            CachedModel loadedModel;
            std::cout << "Loading model into cache...\n";
            fs::path mdlPath;
            if (!database.extractModelSet(prop.model, options.workDir / "models", mdlPath)) {
                std::cerr << "FAILED: Could not extract model: " << prop.model << "\n";
                ++failureCount; continue;
            }
            const fs::path decompiled = options.workDir / "decompiled" / ("model_" + std::to_string(decompileCounter++));
            std::error_code ec2;
            fs::create_directories(decompiled, ec2);
            if (ec2) {
                std::cerr << "FAILED: Could not create decompile directory:\n " << decompiled << "\n";
                ++failureCount; continue;
            }

            const std::string crowbarArgs = "-p " + quoteArg(mdlPath.string()) + " -o " + quoteArg(decompiled.string());
            const int crowbarResult = runCommand(crowbar, crowbarArgs, crowbar.parent_path(), options.verbose);
            if (crowbarResult != 0) {
                std::cerr << "FAILED: Crowbar returned exit code " << crowbarResult << " for " << prop.model << "\n";
                ++failureCount; continue;
            }

            const std::vector<fs::path> qcs = findFiles(decompiled, ".qc");
            for (const fs::path& qc : qcs) {
                const std::vector<std::string> materials = extractCDMaterials(qc);
                for (const std::string& material : materials) {
                    if (std::find(loadedModel.materials.begin(), loadedModel.materials.end(), material) == loadedModel.materials.end())
                        loadedModel.materials.push_back(material);
                }
            }

            const std::vector<fs::path> referenceSmds = findReferenceSMDs(decompiled, mdlPath);
            if (referenceSmds.empty()) {
                std::cerr << "FAILED: No render SMD could be identified.\n"
                          << "Decompiled directory:\n " << decompiled << "\n";
                ++failureCount; continue;
            }
            std::cout << "Selected render SMD(s):\n";
            for (const fs::path& smd : referenceSmds) std::cout << "  " << smd << "\n";
            std::cout << "Found " << referenceSmds.size() << " reference SMD file(s).\n";

            bool geometryLoaded = false;
            for (const fs::path& refSmd : referenceSmds) {
                if (options.verbose) std::cout << "Loading render SMD:\n " << refSmd << "\n";
                SMDMesh bodyMesh;
                if (!loadReferenceSMD(refSmd, bodyMesh)) {
                    std::cerr << " WARNING: Could not load SMD:\n " << refSmd << "\n";
                    continue;
                }
                loadedModel.renderMesh.triangles.insert(loadedModel.renderMesh.triangles.end(),
                                                       bodyMesh.triangles.begin(), bodyMesh.triangles.end());
                geometryLoaded = true;
            }
            if (!geometryLoaded || loadedModel.renderMesh.triangles.empty()) {
                std::cerr << "FAILED: Reference SMDs contained no usable triangles.\n";
                ++failureCount; continue;
            }
            std::cout << "Cached " << loadedModel.renderMesh.triangles.size() << " render triangles.\n";

            const fs::path collisionSmd = findCollisionSMD(decompiled, mdlPath);
            if (collisionSmd.empty()) {
                std::cerr << " WARNING: No collision SMD found for " << prop.model << ".\n";
            } else {
                std::cout << "Collision SMD:\n " << collisionSmd << "\n";
                SMDMesh collisionMesh;
                if (!loadReferenceSMD(collisionSmd, collisionMesh)) {
                    std::cerr << " WARNING: Could not load collision SMD:\n " << collisionSmd << "\n";
                } else {
                    loadedModel.collisionMesh = std::move(collisionMesh);
                    loadedModel.hasCollision = !loadedModel.collisionMesh.triangles.empty();
                    if (loadedModel.hasCollision) {
                        std::cout << "Cached " << loadedModel.collisionMesh.triangles.size() << " collision triangles.\n";
                    }
                }
            }

            cached = modelCache.emplace(cacheKey, std::move(loadedModel)).first;
            std::cout << "Model cached.\n";
        } else {
            std::cout << "Reusing cached geometry for this model.\n";
        }

        SMDMesh mesh = cached->second.renderMesh;
        for (auto& triangle : mesh.triangles) {
            SMDVertex* vertices[] = {&triangle.a, &triangle.b, &triangle.c};
            for (SMDVertex* vertex : vertices) {
                vertex->position = transformPosition(vertex->position, prop, options, coordinateMap);
                vertex->normal = transformNormal(vertex->normal, prop, options, coordinateMap);
                vertex->bone = 0;
            }
        }
        mergedMeshes.push_back(std::move(mesh));

        if (cached->second.hasCollision && !cached->second.collisionMesh.triangles.empty()) {
            SMDMesh collisionMesh = cached->second.collisionMesh;
            transformCollisionMesh(collisionMesh, prop, options, coordinateMap);
            mergedCollisionMesh.triangles.insert(mergedCollisionMesh.triangles.end(),
                                                 collisionMesh.triangles.begin(), collisionMesh.triangles.end());
            std::cout << "Added " << collisionMesh.triangles.size() << " transformed collision triangles.\n";
        } else {
            std::cerr << " WARNING: Prop has no collision geometry: " << prop.model << "\n";
        }

        for (const std::string& material : cached->second.materials) {
            if (std::find(allMaterials.begin(), allMaterials.end(), material) == allMaterials.end())
                allMaterials.push_back(material);
        }
        successfullyMergedEntityIds.insert(prop.entityId);
        ++successCount;
        std::cout << "SUCCESS: prop " << (i+1) << " merged.\n";
    }

    std::cout << "\n==================================================\n"
              << "Merge pass finished\n"
              << " Total props found: " << props.size() << "\n"
              << " Selected: " << selectedProps.size() << "\n"
              << " Excluded: " << excludedCount << "\n"
              << " Successfully merged: " << successCount << "\n"
              << " Failed: " << failureCount << "\n"
              << " Unique models: " << modelCache.size() << "\n"
              << " Collision triangles: " << mergedCollisionMesh.triangles.size() << "\n"
              << "==================================================\n";

    if (mergedMeshes.empty()) {
        std::cerr << "\nNo geometry was successfully merged.\n";
        return 1;
    }
    if (failureCount > 0 && !options.allowFailures) {
        std::cerr << "\nERROR: Some selected props failed.\n"
                  << "Use --allow-failures if you want to compile the successfully merged geometry anyway.\n";
        return 1;
    }

    std::error_code outputEc;
    fs::create_directories(options.outputDirectory, outputEc);
    if (outputEc) {
        std::cerr << "Could not create output directory:\n" << options.outputDirectory << "\n" << outputEc.message() << "\n";
        return 1;
    }

    std::vector<SMDMesh> outputBodies;
    SMDMesh currentBody;
    for (const SMDMesh& mesh : mergedMeshes) {
        for (const SMDTriangle& triangle : mesh.triangles) {
            if (currentBody.triangles.size() >= options.trianglesPerBody) {
                outputBodies.push_back(std::move(currentBody));
                currentBody = SMDMesh{};
            }
            currentBody.triangles.push_back(triangle);
        }
    }
    if (!currentBody.triangles.empty()) outputBodies.push_back(std::move(currentBody));
    if (outputBodies.empty()) {
        std::cerr << "No output SMD bodies were generated.\n";
        return 1;
    }

    std::vector<fs::path> mergedSmdFiles;
    for (size_t bodyIndex = 0; bodyIndex < outputBodies.size(); ++bodyIndex) {
        std::ostringstream filename;
        filename << "merged_prop_" << std::setw(3) << std::setfill('0') << bodyIndex << ".smd";
        const fs::path bodyPath = options.workDir / filename.str();
        std::vector<SMDMesh> oneBody;
        oneBody.push_back(std::move(outputBodies[bodyIndex]));
        if (!writeMergedSMD(bodyPath, oneBody)) {
            std::cerr << "Failed to write merged SMD body:\n " << bodyPath << "\n";
            return 1;
        }
        std::cout << "Wrote body " << (bodyIndex+1) << " / " << outputBodies.size()
                  << " (" << oneBody.front().triangles.size() << " triangles):\n " << bodyPath << "\n";
        mergedSmdFiles.push_back(bodyPath);
    }
    std::cout << "\nGenerated " << mergedSmdFiles.size() << " SMD body file(s).\n";

    fs::path mergedCollisionSmd;
    bool collisionOutputEnabled = !mergedCollisionMesh.triangles.empty();
    if (collisionOutputEnabled) {
        mergedCollisionSmd = options.workDir / "merged_collision.smd";
        std::vector<SMDMesh> collisionMeshes;
        collisionMeshes.push_back(std::move(mergedCollisionMesh));
        if (!writeMergedSMD(mergedCollisionSmd, collisionMeshes)) {
            std::cerr << "Failed to write merged collision SMD:\n " << mergedCollisionSmd << "\n";
            return 1;
        }
        std::cout << "\nWrote merged collision SMD:\n " << mergedCollisionSmd << "\n"
                  << "Collision triangles: " << collisionMeshes.front().triangles.size() << "\n";
    } else {
        std::cerr << "\nWARNING: No collision geometry was found for any successfully merged prop.\n"
                  << "The resulting model will have no generated collision model.\n";
    }

    const std::string finalModelName = "models/merged_props/" + fs::path(options.modelName).filename().string();
    const fs::path mergedQc = options.workDir / "merged_prop.qc";
    std::ofstream qcOut(mergedQc, std::ios::binary);
    if (!qcOut) {
        std::cerr << "Could not create QC:\n" << mergedQc << "\n";
        return 1;
    }
    qcOut << "$modelname \"" << finalModelName << "\"\n";
    qcOut << "$staticprop\n";
    for (size_t bodyIndex = 0; bodyIndex < mergedSmdFiles.size(); ++bodyIndex) {
        qcOut << "$body \"body" << bodyIndex << "\" \"" << mergedSmdFiles[bodyIndex].filename().string() << "\"\n";
    }
    for (const std::string& material : allMaterials) {
        qcOut << "$cdmaterials \"" << material << "\"\n";
    }
    if (collisionOutputEnabled) {
        qcOut << "\n$collisionmodel \"" << mergedCollisionSmd.filename().string() << "\"\n"
              << "{\n"
              << "\t$concave\n"
              << "\t$maxconvexpieces " << options.maxConvexPieces << "\n"
              << "}\n";
    }
    qcOut << "$surfaceprop \"" << options.surfaceProp << "\"\n";
    qcOut << "$sequence \"idle\" \"" << mergedSmdFiles.front().filename().string() << "\" fps " << options.sequenceFps << "\n";
    qcOut.close();
    if (!qcOut) {
        std::cerr << "Failed while writing QC.\n";
        return 1;
    }
    std::cout << "QC written to:\n " << mergedQc << "\n";

    if (collisionOutputEnabled) {
        std::cout << "\nCollision configuration:\n"
                  << " $collisionmodel \"" << mergedCollisionSmd.filename().string() << "\"\n"
                  << " {\n"
                  << "  $concave\n"
                  << "  $maxconvexpieces " << options.maxConvexPieces << "\n"
                  << " }\n";
    }

    const std::string studiomdlArgs = "-game " + quoteArg(options.gameRoot.string()) + " " + quoteArg(mergedQc.string());
    const int studiomdlResult = runCommand(studiomdl, studiomdlArgs, studiomdl.parent_path(), options.verbose);
    if (studiomdlResult != 0) {
        std::cerr << "\nStudioMDL returned exit code " << studiomdlResult << ".\n"
                  << "The VMF will not be modified because the merged model was not compiled successfully.\n";
        return 1;
    }

    const std::string correctedModelLocation = "models/" + finalModelName;
    fs::path compiledMdl = options.gameRoot / correctedModelLocation;
    if (!fs::exists(compiledMdl, ec)) {
        const fs::path alternative = outputModelPath;
        if (fs::exists(alternative, ec)) compiledMdl = alternative;
        else {
            std::cerr << "\nStudioMDL reported success, but the output MDL was not found:\n" << compiledMdl << "\n";
            return 1;
        }
    }
    std::cout << "\nCompiled model:\n " << compiledMdl << "\n";

    if (!options.noVmf) {
        if (!options.keepOriginalProps) {
            uint64_t nextEntityId = 0;
            for (auto& root : roots) {
                removeMergedProps(root, successfullyMergedEntityIds, nextEntityId);
            }
        }

        PropDynamic mergedSource = selectedProps.front();
        VMFObject mergedEntity = mergedSource.object;
        setKey(mergedEntity, "classname", "prop_dynamic");
        setKey(mergedEntity, "model", finalModelName);
        setKey(mergedEntity, "origin", "0 0 0");
        setKey(mergedEntity, "angles", "0 0 0");
        setKey(mergedEntity, "modelscale", "1");
        setKey(mergedEntity, "modelscale_axis", "1 1 1");
        setKey(mergedEntity, "targetname", options.targetName);

        bool addedToWorld = false;
        for (auto& root : roots) {
            if (lowerString(root.name) == "world") {
                root.children.push_back(std::move(mergedEntity));
                addedToWorld = true;
                break;
            }
        }
        if (!addedToWorld) roots.push_back(std::move(mergedEntity));

        std::ofstream vmfOut(outputVMF, std::ios::binary);
        if (!vmfOut) {
            std::cerr << "Could not create output VMF:\n" << outputVMF << "\n";
            return 1;
        }
        for (const auto& root : roots) writeVMFObject(vmfOut, root, 0);
        vmfOut.close();
        if (!vmfOut) {
            std::cerr << "Failed while writing output VMF.\n";
            return 1;
        }
    }

    std::cout << "\n====================================================\n"
              << " SUCCESS\n"
              << "====================================================\n\n"
              << "Props found: " << props.size() << "\n"
              << "Props selected: " << selectedProps.size() << "\n"
              << "Props excluded: " << excludedCount << "\n"
              << "Successfully merged: " << successCount << "\n"
              << "Failed: " << failureCount << "\n"
              << "Unique models: " << modelCache.size() << "\n"
              << "SMD body files: " << mergedSmdFiles.size() << "\n"
              << "Collision: " << (collisionOutputEnabled ? "enabled" : "NOT AVAILABLE") << "\n"
              << "Max convex pieces: " << options.maxConvexPieces << "\n\n"
              << "Coordinate map: " << options.coordMap << "\n"
              << "Rotation: " << (options.applyRotation ? "enabled" : "disabled") << "\n\n"
              << "Output model:\n " << compiledMdl << "\n\n";
    if (!options.noVmf) std::cout << "Output VMF:\n " << outputVMF << "\n\n";
    std::cout << "Work directory:\n " << options.workDir << "\n\n";
    if (options.keepWork) std::cout << "Intermediate files were preserved.\n\n";
    else std::cout << "Intermediate files are in the work directory.\nUse --keep-work to preserve them between runs.\n\n";

    return 0;
}