import javafx.application.Application;
import javafx.application.Platform;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.stage.Stage;

/**
 * @brief Main application class for the PIC Controller JavaFX application.
 * 
 * Handles primary stage setup and serves as the entry point for the application.
 */

public class Main extends Application {

    /**
     * @brief Entry point for the Java application
     * @param args Command line arguments (not used)
     */
    public static void main(String[] args) {
        launch(args);
    }

    /**
     * @brief JavaFX application start method
     * @param primaryStage The main application window
     * @throws Exception if FXML loading fails
     * 
     * Initializes the GUI from FXML, sets up the controller, and configures:
     * - Window dimensions (700x700)
     * - Window title ("PIC Controller")
     * - Clean shutdown handling on window close
     */
    @Override
    public void start(Stage primaryStage) throws Exception {
        // Load the FXML file
        FXMLLoader loader = new FXMLLoader(getClass().getResource("app.fxml"));
        Parent root = loader.load();
        Controller controller = loader.getController();

        // Set up the stage
        primaryStage.setTitle("PIC Controller");
		primaryStage.setScene(new Scene(root, 700, 700));
		primaryStage.setWidth(700);
		primaryStage.setHeight(700);
		primaryStage.show();

        /**
         * @brief Close request handler for graceful shutdown/cleanup
         * 
         * Ensures:
         * - Controller cleanup is performed
         * - JavaFX platform exits properly
         * - JVM terminates completely
         */
		primaryStage.setOnCloseRequest(e -> {
        if (controller != null) {
            controller.stopServer();
        }
        Platform.exit();
        System.exit(0);
    });
    }
}